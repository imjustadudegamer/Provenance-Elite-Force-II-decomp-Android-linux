/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */

#include "stdafx.h"
#include "PeerLobby.h"
#include "CreatePage.h"
#include "LobbyWizard.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CCreatePage * CreatePage;


IMPLEMENT_DYNCREATE(CCreatePage, CPropertyPage)

CCreatePage::CCreatePage() : CPropertyPage(CCreatePage::IDD)
{
	m_name = _T("My Server");
	m_maxPlayers = 8;
}

CCreatePage::~CCreatePage()
{
}

void CCreatePage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_NAME, m_name);
	DDX_Text(pDX, IDC_MAX_PLAYERS, m_maxPlayers);
	DDV_MinMaxInt(pDX, m_maxPlayers, 2, 9999);
}


BEGIN_MESSAGE_MAP(CCreatePage, CPropertyPage)
END_MESSAGE_MAP()

BOOL CCreatePage::OnSetActive() 
{
	Wizard->SetWizardButtons(PSWIZB_BACK | PSWIZB_NEXT);

	Wizard->m_hosting = TRUE;

	return CPropertyPage::OnSetActive();
}

LRESULT CCreatePage::OnWizardBack() 
{
	Wizard->m_hosting = FALSE;

	return CPropertyPage::OnWizardBack();
}

static PEERBool createStagingSuccess;
static void CreateStagingRoomCallback
(
	PEER peer,
	PEERBool success,
	PEERJoinResult result,
	RoomType roomType,
	void * param
)
{
	createStagingSuccess = success;

	GSI_UNUSED(param);
	GSI_UNUSED(roomType);
	GSI_UNUSED(result);
	GSI_UNUSED(peer);
}

LRESULT CCreatePage::OnWizardNext() 
{
	UpdateData();

	Wizard->StartHourglass();
	peerCreateStagingRoom(Wizard->m_peer, m_name, m_maxPlayers, NULL, CreateStagingRoomCallback, NULL, PEERTrue);
	Wizard->StopHourglass();

	return CPropertyPage::OnWizardNext();
}
