/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */

#include "stdafx.h"
#include "Voice2BuddyMFC.h"
#include "VoiceSessionDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define THINK_TIMER_ID      100+2
#define THINK_TIMER_DELAY   20


struct VoicePacket
{
	GVFrameStamp  mFrameStamp;
	int           mDataLength;	
	GVByte        mBuffer[1024];  
};
const int gVoicePacketHeaderSize = sizeof(GVFrameStamp)+sizeof(int);


CVoiceSessionDlg::CVoiceSessionDlg(CWnd* pParent )
	: CDialog(CVoiceSessionDlg::IDD, pParent)
{
	m_DisplayText = _T("");
}


void CVoiceSessionDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SPEAKING2, m_RemoteSpeaking);
	DDX_Control(pDX, IDC_SPEAKING1, m_LocalSpeaking);
	DDX_Text(pDX, IDC_DISPLAYTEXT, m_DisplayText);
}


BEGIN_MESSAGE_MAP(CVoiceSessionDlg, CDialog)
	ON_WM_TIMER()
END_MESSAGE_MAP()

void CVoiceSessionDlg::OnTimer(UINT nIDEvent) 
{
	static bool inTimer = false;
	if (inTimer)
		return;
	
	inTimer = true;

	gvThink();

	NNThink();

	if (m_Socket)
		gt2Think(m_Socket);

	if (!m_Connection || (gt2GetConnectionState(m_Connection) != GT2Connected))
	{
		inTimer = false;
		return;
	}

	static bool wasLocalSpeaking = false;
	static bool wasRemoteSpeaking = false;

	bool isLocalSpeaking  = (GVTrue == gvIsSourceTalking(m_SetupInfo.m_PlaybackDevice, 0));
	bool isRemoteSpeaking = (GVTrue == gvIsSourceTalking(m_SetupInfo.m_PlaybackDevice, m_RemoteAddr));
	
	if (isLocalSpeaking != wasLocalSpeaking)
	{
		wasLocalSpeaking = isLocalSpeaking;
		m_LocalSpeaking.ShowWindow(isLocalSpeaking ? SW_SHOW:SW_HIDE);
	}
	if (isRemoteSpeaking != wasRemoteSpeaking)
	{
		wasRemoteSpeaking = isRemoteSpeaking;
		m_RemoteSpeaking.ShowWindow(isRemoteSpeaking ? SW_SHOW:SW_HIDE);
	}

	int aBytesAvailable = gvGetAvailableCaptureBytes(m_SetupInfo.m_CaptureDevice);
	if (aBytesAvailable > 0)
	{

		VoicePacket aVoicePacket;
		memset(&aVoicePacket, 0, sizeof(aVoicePacket));
		aVoicePacket.mDataLength = sizeof(aVoicePacket.mBuffer);

		GVScalar    aVolume; 

		GVBool gotPacket = gvCapturePacket(m_SetupInfo.m_CaptureDevice, aVoicePacket.mBuffer, &aVoicePacket.mDataLength, &aVoicePacket.mFrameStamp, &aVolume);
		if (gotPacket == GVTrue)
		{

			int aTotalPacketSize = gVoicePacketHeaderSize + aVoicePacket.mDataLength;
			gt2Send(m_Connection, (GT2Byte*)&aVoicePacket, aTotalPacketSize, GT2False);
		}
	}

	inTimer = false;
	
	CDialog::OnTimer(nIDEvent);
}


void GT2SocketErrorCallback(GT2Socket theSocket)
{
	CVoiceSessionDlg* aDialog = (CVoiceSessionDlg*)gt2GetSocketData(theSocket);
	aDialog->m_DisplayText = "GT2SocketErrorCallback.  Connection Closed";
	aDialog->UpdateData(FALSE);
}

void GT2ConnectedCallback(GT2Connection theConnection, GT2Result theResult, unsigned char* theMessage, int theLength)
{
	GT2Socket aSocket = gt2GetConnectionSocket(theConnection);
	CVoiceSessionDlg* aDialog = (CVoiceSessionDlg*)gt2GetSocketData(aSocket);

	if (theResult != GT2Success)
	{
		aDialog->MessageBox("GT2ConnectedCallback returned failure!");
		aDialog->PostMessage(WM_CLOSE, 0, 0);
	}
	else
	{
		aDialog->m_Connection = theConnection;
		aDialog->m_DisplayText = "Session established.\r\n(You may begin speaking)";
		aDialog->UpdateData(FALSE);

		if (aDialog->m_SetupInfo.m_CaptureDevice != NULL)
			gvStartDevice(aDialog->m_SetupInfo.m_CaptureDevice, GV_CAPTURE);
		if (aDialog->m_SetupInfo.m_PlaybackDevice != NULL)
			gvStartDevice(aDialog->m_SetupInfo.m_PlaybackDevice, GV_PLAYBACK);
	}
	GSI_UNUSED(theMessage);
	GSI_UNUSED(theLength);
	GSI_UNUSED(theResult);
}

void GT2ReceivedCallback(GT2Connection theConnection, unsigned char* theBuffer, int theLength, int wasReliable)
{
	CVoiceSessionDlg* aDlg = (CVoiceSessionDlg*)gt2GetSocketData(gt2GetConnectionSocket(theConnection));

	VoicePacket* aVoicePacket = (VoicePacket*)theBuffer;

	assert(theLength >= gVoicePacketHeaderSize);
	assert(theLength == (gVoicePacketHeaderSize + aVoicePacket->mDataLength));

	GVSource aSource = gt2GetRemoteIP(theConnection);
	gvPlayPacket(aDlg->m_SetupInfo.m_PlaybackDevice, aVoicePacket->mBuffer, aVoicePacket->mDataLength, aSource, aVoicePacket->mFrameStamp, 0);
	OutputDebugString("Playing voice packet\r\n");
	GSI_UNUSED(wasReliable);
	GSI_UNUSED(theLength);
}

GT2Bool GT2UnrecognizedMessageCallback(GT2Socket theSocket, unsigned int theIp, unsigned short thePort, GT2Byte* theData, int theLength)
{
	static unsigned char aNNHeader[NATNEG_MAGIC_LEN] = 
				{ NN_MAGIC_0, NN_MAGIC_1, NN_MAGIC_2,
				NN_MAGIC_3, NN_MAGIC_4, NN_MAGIC_5 };

	if (theLength < NATNEG_MAGIC_LEN)
	{
		OutputDebugString("\tDiscarded message (too small)\r\n");
		return GT2False;
	}

	if (0==memcmp(theData, aNNHeader,NATNEG_MAGIC_LEN))
	{
		sockaddr_in anAddr;
		anAddr.sin_family      = AF_INET;
		anAddr.sin_port        = htons(thePort);
		anAddr.sin_addr.s_addr = theIp;
		NNProcessData((char*)theData, theLength, &anAddr);
		OutputDebugString("\tProcessed NN message\r\n");
		return GT2True;
	}

	OutputDebugString("\tDiscarded message (not recognized)\r\n");
	GSI_UNUSED(theSocket);
	return GT2False;
}

void GT2ClosedCallback(GT2Connection theConnection, GT2CloseReason theReason)
{
	GT2Socket aSocket = gt2GetConnectionSocket(theConnection);
	CVoiceSessionDlg* aDialog = (CVoiceSessionDlg*)gt2GetSocketData(aSocket);

	aDialog->m_DisplayText = "Connection closed. (GT2ClosedCallback)";
	aDialog->UpdateData(FALSE);
	GSI_UNUSED(theReason);
}

void GT2ConnectAttemptCallback(GT2Socket theSocket,  GT2Connection theConnection,  unsigned int theIp,  unsigned short thePort,  int theLatency,  GT2Byte * theMessage,  int theLength)
{
	CVoiceSessionDlg* aDialog = (CVoiceSessionDlg*)gt2GetSocketData(theSocket);

	GT2ConnectionCallbacks aCallbackList;
	aCallbackList.received  = GT2ReceivedCallback;
	aCallbackList.connected = GT2ConnectedCallback;  
	aCallbackList.closed    = GT2ClosedCallback;

	aDialog->m_Connection = theConnection;
	gt2Accept(theConnection, &aCallbackList);

	aDialog->m_RemoteAddr = gt2GetRemoteIP(theConnection);

	if (aDialog->m_SetupInfo.m_CaptureDevice != NULL)
		gvStartDevice(aDialog->m_SetupInfo.m_CaptureDevice, GV_CAPTURE);
	if (aDialog->m_SetupInfo.m_PlaybackDevice != NULL)
		gvStartDevice(aDialog->m_SetupInfo.m_PlaybackDevice, GV_PLAYBACK);

	aDialog->m_DisplayText = "Session established.\r\n(You may begin speaking)";
	aDialog->UpdateData(FALSE);
	GSI_UNUSED(theMessage);
	GSI_UNUSED(theLength);
	GSI_UNUSED(theIp);
	GSI_UNUSED(thePort);
	GSI_UNUSED(theLatency);
}

void NNProgressCallback(NegotiateState theState, void* theParam)
{
	CVoiceSessionDlg* aDialog = (CVoiceSessionDlg*)theParam;
	aDialog->m_DisplayText.Format("NNProgressCallback: %d\r\n", theState);
	aDialog->UpdateData(FALSE);
}

void NNCompletedCallback(NegotiateResult theResult, SOCKET theSocket, sockaddr_in* theAddr, void* theParam)
{
	CVoiceSessionDlg* aDialog = (CVoiceSessionDlg*)theParam;
	
	if (theResult == nr_success)
	{
		CString aAddrString;
		aAddrString.Format("%s:%d", inet_ntoa(theAddr->sin_addr), ntohs(theAddr->sin_port));
		aDialog->m_DisplayText = "NN Successful";

		if (aDialog->m_IsHost)
		{
			gt2Listen(aDialog->m_Socket, GT2ConnectAttemptCallback);
			aDialog->m_DisplayText = "Listening for GT2 connection";
		}
		else
		{
			aDialog->m_RemoteAddr = theAddr->sin_addr.s_addr;

			GT2ConnectionCallbacks aCallbackList;
			aCallbackList.connected = GT2ConnectedCallback;  
			aCallbackList.received  = GT2ReceivedCallback;   
			aCallbackList.closed    = GT2ClosedCallback;

			int aTimeout = 0;
			GT2Result aResult = gt2Connect(aDialog->m_Socket, &aDialog->m_Connection, aAddrString, NULL, 0, aTimeout, &aCallbackList, GT2False);
			if (aResult != GT2Success)
			{
				aDialog->m_DisplayText = "Failed on gt2Connect";
			}
			aDialog->m_DisplayText = "Connecting with GT2";
		}
	}
	else
	{
		aDialog->m_DisplayText = "Failed to negotiate a connection.";
	}

	aDialog->UpdateData(FALSE);
	GSI_UNUSED(theSocket);
}


BOOL CVoiceSessionDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	m_Socket     = NULL;
	m_Connection = NULL;

	GT2Result aResult = gt2CreateSocket(&m_Socket, NULL, 0, 0, GT2SocketErrorCallback);
	if (aResult != GT2Success)
	{
		MessageBox("Failed on gt2CreateSocket");
		PostMessage(WM_CLOSE, 0, 0);
		return TRUE;
	}

	gt2SetSocketData(m_Socket, this);
	gt2SetUnrecognizedMessageCallback(m_Socket, GT2UnrecognizedMessageCallback);

	SOCKET aWinSocket = gt2GetSocketSOCKET(m_Socket);
	NNBeginNegotiationWithSocket(aWinSocket, m_NNCookie, m_IsHost, NNProgressCallback, NNCompletedCallback, this);

	m_DisplayText = "Establishing connection...";
	UpdateData(FALSE);

	SetTimer(THINK_TIMER_ID, THINK_TIMER_DELAY, NULL);

	return TRUE;  
}

BOOL CVoiceSessionDlg::DestroyWindow() 
{
	NNFreeNegotiateList();

	if (m_Socket)
		gt2CloseSocket(m_Socket);

	if (m_SetupInfo.m_CaptureDevice != NULL)
		gvStopDevice(m_SetupInfo.m_CaptureDevice, GV_CAPTURE);
	if (m_SetupInfo.m_PlaybackDevice != NULL)
		gvStopDevice(m_SetupInfo.m_PlaybackDevice, GV_PLAYBACK);

	KillTimer(THINK_TIMER_ID);


	return CDialog::DestroyWindow();
}
