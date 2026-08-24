/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#include "gvPS2Spu2.h"
#if !defined(GV_NO_PS2_SPU2)
#include "gvDevice.h"
#include "gvCodec.h"
#include "gvSource.h"
#include "gvUtil.h"
#include <libsdr.h>
#include <sif.h>

#if !defined(_PS2)
#error This file should only be used with the PlayStation2
#endif



#define bss_align(val) \
 __attribute__ ((aligned(val))) __attribute__ ((section (".bss")))
#define LIMIT(x, minx, maxx)  ((x) < (minx) ? (minx) : ((x) > (maxx) ? (maxx) : (x)))

#define AUTODMA_CH			1

#define SPU_BLOCK_SIZE			 512	
#define IOP_BUFF_SIZE		   12288	
#define SPU_BUFF_SIZE			2048	

#define CORE0_INPUT_L	(0x2000)
#define CORE0_INPUT_R	(0x2200)
#define CORE1_INPUT_L	(0x2400)
#define CORE1_INPUT_R	(0x2600)
#if (AUTODMA_CH == 0)
#define CORE_INPUT_L   CORE0_INPUT_L
#define CORE_INPUT_R   CORE0_INPUT_R
#else
#define CORE_INPUT_L   CORE1_INPUT_L
#define CORE_INPUT_R   CORE1_INPUT_R
#endif

#define EEBUFFER_SAMPLE_COUNT (IOP_BUFF_SIZE/4)	

typedef struct
{
	GVBool m_playing;
	GVScalar m_playbackVolume;
	GVFrameStamp m_playbackClock;
	GVISourceList m_playbackSources;
	GVSample * m_playbackBuffer;
	int m_playbackBufferPos;
} GVIPS2Spu2Data;

const GVDeviceID GVPS2Spu2DeviceID = -1;
static GVIDevice * GVIPS2Spu2Device;

static gsi_u16  EEBuffer[IOP_BUFF_SIZE]	bss_align(64);
static int	IOPBuffer			= 0;	
static int	gEEStreanIntr		= 0;	
static int  gEEDataSampleCount	= 0;	

static int DMA_EE_To_IOP(int dst, u_char *src, int size)
{
	sceSifDmaData transData;
	int did;

	assert((((gsi_u32)src) & 0x3f) == 0);  
		
	if (size <= 0) {
		return 0;
	}

	transData.data = (u_int)src;
	transData.addr = (u_int)dst;
	transData.size = (unsigned int)size;
	transData.mode = 0; 
	FlushCache(0);

	did = (int)sceSifSetDma( &transData, 1 );

	while (sceSifDmaStat((unsigned int)did) >= 0)
		;

	return size;
}

static int cbEEStreamTransfer( void* common )
{
	gEEStreanIntr = 1;
	GSI_UNUSED(common);
	return 0;
}

static void EEStreamClearSPUBuffer()
{
	memset (EEBuffer, 0, IOP_BUFF_SIZE);
	FlushCache(0);
	DMA_EE_To_IOP(	IOPBuffer,							
					(u_char *)EEBuffer,					
					IOP_BUFF_SIZE);	

	DMA_EE_To_IOP(	IOPBuffer + IOP_BUFF_SIZE,							
					(u_char *)EEBuffer,					
					IOP_BUFF_SIZE);	

    sceSdRemote(1, rSdVoiceTrans, AUTODMA_CH, SD_TRANS_MODE_WRITE | SD_TRANS_BY_DMA,
		 IOPBuffer, CORE_INPUT_L, 0x800 );
    sceSdRemote(1, rSdVoiceTrans, AUTODMA_CH, SD_TRANS_MODE_WRITE | SD_TRANS_BY_DMA,
		 IOPBuffer + IOP_BUFF_SIZE, CORE_INPUT_R, 0x800 );

    sceSdRemote (1, rSdVoiceTransStatus, AUTODMA_CH, SD_TRANS_STATUS_WAIT);
}

GVBool gviPS2Spu2Startup(void)
{
	GVIPS2Spu2Device = NULL;

	if(sceSdRemoteInit() != 0)
		return GVFalse;
	if(sceSdRemote(1, rSdInit, SD_INIT_COLD) != 0)
		return GVFalse;
	if(sceSifInitIopHeap() != 0)
		return GVFalse;

	return GVTrue;
}

void gviPS2Spu2Cleanup(void)
{
	if(GVIPS2Spu2Device)
		gviFreeDevice(GVIPS2Spu2Device);
}

static void	AudioUpSample(gsi_i16* Dest48Khz, const gsi_i16* Src8Khz, int num8Khzsamples)
{
#if(0)
	while (num8Khzsamples)
	{
		int a = Src8Khz[0];


		Dest48Khz[0]	= a;
		Dest48Khz[1]	= a;
		Dest48Khz[2]	= a;
		Dest48Khz[3]	= a;
		Dest48Khz[4]	= a;
		Dest48Khz[5]	= a;

		Dest48Khz		+=6;
		Src8Khz			++;
		num8Khzsamples--;
	}
#elif (1)
	int a=0,b=0;
	while (num8Khzsamples - 1)
	{

		a = Src8Khz[0];
		b = Src8Khz[1];


		Dest48Khz[0]	= (gsi_i16)a;
		Dest48Khz[1]	= (gsi_i16)((5*a)/6 +(1*b)/6);
		Dest48Khz[2]	= (gsi_i16)((4*a)/6 +(2*b)/6);
		Dest48Khz[3]	= (gsi_i16)((3*a)/6 +(3*b)/6);
		Dest48Khz[4]	= (gsi_i16)((2*a)/6 +(4*b)/6);
		Dest48Khz[5]	= (gsi_i16)((1*a)/6 +(5*b)/6);

		Dest48Khz		+=6;
		Src8Khz			++;
		num8Khzsamples--;
	}
		Dest48Khz[0]	= (gsi_i16)b;
		Dest48Khz[1]	= (gsi_i16)b;
		Dest48Khz[2]	= (gsi_i16)b;
		Dest48Khz[3]	= (gsi_i16)b;
		Dest48Khz[4]	= (gsi_i16)b;
		Dest48Khz[5]	= (gsi_i16)b;

#else




	float a = (float)Src8Khz[ 0];
	float b = (float)Src8Khz[ 0];
	float c = (float)Src8Khz[ 1];
	float d = (float)Src8Khz[ 2];
	float v;

	Dest48Khz[0]	=Src8Khz[ 0];	
	Dest48Khz[1]	=(gsi_u16)(a * -0.058f  + b * 0.938f  + c * 0.132f  + d * -0.012f );			
	Dest48Khz[2]	=(gsi_u16)(a * -0.074f  + b * 0.778f  + c * 0.333f  + d * -0.037f );			
	Dest48Khz[3]	=(gsi_u16)(a * -0.063f  + b * 0.563f  + c * 0.563f  + d * -0.063f );			
	Dest48Khz[4]	=(gsi_u16)(a * -0.037f  + b * 0.333f  + c * 0.778f  + d * -0.074f );			
	Dest48Khz[5]	=(gsi_u16)(a * -0.012f  + b * 0.132f  + c * 0.938f  + d * -0.058f );			

	Dest48Khz		+=6;
	Src8Khz			++;
	num8Khzsamples-=3;

	while (num8Khzsamples)
	{
		a = (float)Src8Khz[-1];
		b = (float)Src8Khz[ 0];
		c = (float)Src8Khz[ 1];
		d = (float)Src8Khz[ 2];

		Dest48Khz[0]	=Src8Khz[ 0];	

		v = (a * -0.058f  + b * 0.938f  + c * 0.132f  + d * -0.012f );
		Dest48Khz[1]	=(gsi_u16)	LIMIT(v,0.0f,65535.0f);		
		
		v = (a * -0.074f  + b * 0.778f  + c * 0.333f  + d * -0.037f );	
		Dest48Khz[2]	=(gsi_u16)	LIMIT(v,0.0f,65535.0f);	
		
		v = (a * -0.063f  + b * 0.563f  + c * 0.563f  + d * -0.063f );
		Dest48Khz[3]	=(gsi_u16)	LIMIT(v,0.0f,65535.0f);		
		
		v= (a * -0.037f  + b * 0.333f  + c * 0.778f  + d * -0.074f );
		Dest48Khz[4]	=(gsi_u16)	LIMIT(v,0.0f,65535.0f);		
		
		v=(a * -0.012f  + b * 0.132f  + c * 0.938f  + d * -0.058f );
		Dest48Khz[5]	=(gsi_u16)	LIMIT(v,0.0f,65535.0f);		

		Dest48Khz		+=6;
		Src8Khz			++;
		num8Khzsamples--;
	}

	a = (float)Src8Khz[ 0];
	b = (float)Src8Khz[ 0];
	c = (float)Src8Khz[ 1];
	d = (float)Src8Khz[ 1];

	Dest48Khz[0]	=Src8Khz[ 0];	
	Dest48Khz[1]	=(gsi_u16)(a * -0.058f  + b * 0.938f  + c * 0.132f  + d * -0.012f );			
	Dest48Khz[2]	=(gsi_u16)(a * -0.074f  + b * 0.778f  + c * 0.333f  + d * -0.037f );			
	Dest48Khz[3]	=(gsi_u16)(a * -0.063f  + b * 0.563f  + c * 0.563f  + d * -0.063f );			
	Dest48Khz[4]	=(gsi_u16)(a * -0.037f  + b * 0.333f  + c * 0.778f  + d * -0.074f );			
	Dest48Khz[5]	=(gsi_u16)(a * -0.012f  + b * 0.132f  + c * 0.938f  + d * -0.058f );			

	Dest48Khz		+=6;
	Src8Khz			++;

	a = (float)Src8Khz[ 0];
	b = (float)Src8Khz[ 0];
	c = (float)Src8Khz[ 0];
	d = (float)Src8Khz[ 0];

	Dest48Khz[0]	=Src8Khz[ 0];	
	Dest48Khz[1]	=(gsi_u16)(a * -0.058f  + b * 0.938f  + c * 0.132f  + d * -0.012f );			
	Dest48Khz[2]	=(gsi_u16)(a * -0.074f  + b * 0.778f  + c * 0.333f  + d * -0.037f );			
	Dest48Khz[3]	=(gsi_u16)(a * -0.063f  + b * 0.563f  + c * 0.563f  + d * -0.063f );			
	Dest48Khz[4]	=(gsi_u16)(a * -0.037f  + b * 0.333f  + c * 0.778f  + d * -0.074f );			
	Dest48Khz[5]	=(gsi_u16)(a * -0.012f  + b * 0.132f  + c * 0.938f  + d * -0.058f );			

	Dest48Khz		+=6;
	Src8Khz			++;


#endif

}

static int		EEStreamDataPush(const gsi_u16* data, int num8Khzsamples)
{
	int remain48Khz = EEBUFFER_SAMPLE_COUNT - gEEDataSampleCount;

	if (num8Khzsamples *6  > remain48Khz)
	{
		num8Khzsamples = remain48Khz/6;
	}

	assert(EEBUFFER_SAMPLE_COUNT >= num8Khzsamples*6);

	AudioUpSample((gsi_i16*)&EEBuffer[gEEDataSampleCount],(const gsi_i16*)data,num8Khzsamples);

	gEEDataSampleCount+=num8Khzsamples*6;

	return num8Khzsamples;
}

static void EEStreamConvertAndTransfer(GVIDevice * device, int which)
{
	GVIPS2Spu2Data * data = (GVIPS2Spu2Data *)device->m_data;
	GVBool wroteToBuffer;
	int samplesPushed;

static int BlockSize= 256;	
static int EEMul	= 2;
static int IOPMul	= 4;

	int		iopmem;
	u_char *eemem;
	int i;

	if(data->m_playbackBufferPos)
	{
		EEStreamDataPush(data->m_playbackBuffer + data->m_playbackBufferPos, GVISamplesPerFrame - data->m_playbackBufferPos);
		data->m_playbackBufferPos = 0;
	}

	do
	{
		wroteToBuffer = gviWriteSourcesToBuffer(data->m_playbackSources, data->m_playbackClock, data->m_playbackBuffer, 1);

		if(!wroteToBuffer)
			memset(data->m_playbackBuffer, 0, (unsigned int)GVIBytesPerFrame);

		if(device->m_playbackFilterCallback)
			device->m_playbackFilterCallback(device, data->m_playbackBuffer, data->m_playbackClock);

		samplesPushed = EEStreamDataPush(data->m_playbackBuffer, GVISamplesPerFrame);

		data->m_playbackClock++;
	}
	while(gEEDataSampleCount < EEBUFFER_SAMPLE_COUNT);

	if(samplesPushed < GVISamplesPerFrame)
		data->m_playbackBufferPos = samplesPushed;
	else
		data->m_playbackBufferPos = 0;

	gEEDataSampleCount = 0;	

	iopmem = IOPBuffer + which * IOP_BUFF_SIZE;
	eemem  = (u_char *)(&EEBuffer[0]);
	for (i=0; i< EEBUFFER_SAMPLE_COUNT;i+= BlockSize)
	{		
		DMA_EE_To_IOP(	iopmem,				
						eemem,				
						BlockSize*2);	

		DMA_EE_To_IOP(	iopmem+BlockSize*2,				
						eemem,				
						BlockSize*2);	

		iopmem  += BlockSize * IOPMul;		
		eemem	+= BlockSize * EEMul; 
	}
}

void gviPS2Spu2Think(void)
{
	GVIPS2Spu2Data * data;
	int v;
	int which;

	if(!GVIPS2Spu2Device)
		return;

	data = (GVIPS2Spu2Data *)GVIPS2Spu2Device->m_data;

	if(!data->m_playing)
		return;

	if(!gEEStreanIntr)
		return;

	gEEStreanIntr = 0;

	v	= sceSdRemote(1, rSdBlockTransStatus ,AUTODMA_CH,0);

	which = 1 - (v >>24);

	EEStreamConvertAndTransfer(GVIPS2Spu2Device, which);
}

int gviPS2Spu2ListDevices(GVDeviceInfo devices[], int maxDevices, GVDeviceType types)
{
	if(maxDevices < 1)
		return 0;

	if(!(types & GV_PLAYBACK))
		return 0;

	devices[0].m_id = GVPS2Spu2DeviceID;
	strcpy(devices[0].m_name, "System Sound");
	devices[0].m_deviceType = GV_PLAYBACK;
	devices[0].m_defaultDevice = (GVDeviceType)0; 
	devices[0].m_hardwareType = GVHardwarePS2Spu2;

	return 1;
}

static GVBool gviPS2Spu2StartDevice(GVIDevice * device, GVDeviceType type)
{
	GVIPS2Spu2Data * data = (GVIPS2Spu2Data *)device->m_data;

	if(!(type & GV_PLAYBACK))
		return GVFalse;

	data->m_playbackClock = 0;

	data->m_playing = GVTrue;

	return GVTrue;
}

static void gviPS2Spu2StopDevice(GVIDevice * device, GVDeviceType type)
{
	GVIPS2Spu2Data * data = (GVIPS2Spu2Data *)device->m_data;

	if(!(type & GV_PLAYBACK))
		return;

	data->m_playing = GVFalse;

	gviClearSourceList(data->m_playbackSources);

	EEStreamClearSPUBuffer();
}

static GVBool gviPS2Spu2IsDeviceStarted(GVIDevice * device, GVDeviceType type)
{
	GVIPS2Spu2Data * data = (GVIPS2Spu2Data *)device->m_data;

	if(type == GV_PLAYBACK)
		return data->m_playing;
	return GVFalse;
}

static void gviPS2Spu2SetDeviceVolume(GVIDevice * device, GVDeviceType type, GVScalar volume)
{
	GVIPS2Spu2Data * data = (GVIPS2Spu2Data *)device->m_data;
	gsi_i16 sVol;

	if(!(type & GV_PLAYBACK))
		return;

	data->m_playbackVolume = volume;

	sVol = (gsi_i16)(volume * 0x3FFF);
    sceSdRemote(1, rSdSetParam, AUTODMA_CH | SD_P_BVOLL, sVol);
    sceSdRemote(1, rSdSetParam, AUTODMA_CH | SD_P_BVOLR, sVol);
}

static GVScalar gviPS2Spu2GetDeviceVolume(GVIDevice * device, GVDeviceType type)
{
	GVIPS2Spu2Data * data = (GVIPS2Spu2Data *)device->m_data;

	if(!(type & GV_PLAYBACK))
		return 0;

	return data->m_playbackVolume;
}

static void gviPS2Spu2PlayPacket(GVIDevice * device, const GVByte * packet, int len, GVSource source, GVFrameStamp frameStamp, GVBool mute)
{
	GVIPS2Spu2Data * data = (GVIPS2Spu2Data *)device->m_data;

	if(!data->m_playing)
		return;

	gviAddPacketToSourceList(data->m_playbackSources, packet, len, source, frameStamp, mute, data->m_playbackClock);
}

static GVBool gviPS2Spu2IsSourceTalking(GVDevice device, GVSource source)
{
	GVIPS2Spu2Data * data = (GVIPS2Spu2Data *)device->m_data;

	if(!data->m_playing)
		return GVFalse;

	return gviIsSourceTalking(data->m_playbackSources, source);
}

static int gviPS2Spu2ListTalkingSources(GVDevice device, GVSource sources[], int maxSources)
{
	GVIPS2Spu2Data * data = (GVIPS2Spu2Data *)device->m_data;

	if(!data->m_playing)
		return GVFalse;

	return gviListTalkingSources(data->m_playbackSources, sources, maxSources);
}

static void gviPS2Spu2FreeDevice(GVIDevice * device)
{
	GVIPS2Spu2Data * data = (GVIPS2Spu2Data *)device->m_data;

	EEStreamClearSPUBuffer();

    sceSifFreeIopHeap((void *)IOPBuffer);

	IOPBuffer		= (int)NULL;

	sceSdRemoteCallbackQuit();
	sceSdRemote(1, rSdSetTransIntrHandler,AUTODMA_CH,NULL, NULL);


	if(data->m_playbackSources)
		gviFreeSourceList(data->m_playbackSources);
	gsifree(data->m_playbackBuffer);

	gviFreeDevice(device);

	GVIPS2Spu2Device = NULL;
}

static GVBool gviPS2Spu2InitDevice(GVIDevice * device, int deviceIndex, GVDeviceType type)
{
	GVIPS2Spu2Data * data = (GVIPS2Spu2Data *)device->m_data;

	data->m_playbackSources = gviNewSourceList();
	if(!data->m_playbackSources)
		return GVFalse;

	data->m_playbackBuffer = (GVSample *)gsimalloc((unsigned int)GVIBytesPerFrame);
	if(!data->m_playbackBuffer)
	{
		gviFreeSourceList(data->m_playbackSources);
		return GVFalse;
	}

	IOPBuffer		= (int)sceSifAllocIopHeap(IOP_BUFF_SIZE*2);
	assert(IOPBuffer	);

	sceSdRemoteCallbackInit(5);
	sceSdRemote(1, rSdSetTransIntrHandler,AUTODMA_CH,cbEEStreamTransfer, NULL);


	gEEStreanIntr = 0;	

	EEStreamClearSPUBuffer();



	sceSdRemote(1, rSdBlockTrans,	AUTODMA_CH,
    								(SD_TRANS_MODE_WRITE| SD_BLOCK_LOOP),
		 							IOPBuffer,				
									IOP_BUFF_SIZE*2,		
									IOPBuffer				
				);

    sceSdRemote(1, rSdSetParam, AUTODMA_CH | SD_P_BVOLL, 0x3FFF);
    sceSdRemote(1, rSdSetParam, AUTODMA_CH | SD_P_BVOLR, 0x3FFF);
    sceSdRemote(1, rSdSetParam, AUTODMA_CH | SD_P_MVOLL, 0x3FFF);
    sceSdRemote(1, rSdSetParam, AUTODMA_CH | SD_P_MVOLR, 0x3FFF);

	data->m_playbackVolume = 1.0;
	GSI_UNUSED(deviceIndex);
	GSI_UNUSED(type);
	return GVTrue;
}

GVDevice gviPS2Spu2NewDevice(GVDeviceID deviceID, GVDeviceType type)
{
	GVIDevice * device;
	GVBool result;

	if(GVIPS2Spu2Device)
		return NULL;

	if(deviceID != GVPS2Spu2DeviceID)
		return NULL;

	if(type != GV_PLAYBACK)
		return NULL;

	device = gviNewDevice(deviceID, GVHardwarePS2Spu2, type, sizeof(GVIPS2Spu2Data));
	if(!device)
		return NULL;

	result = gviPS2Spu2InitDevice(device, deviceID, type);
	if(result == GVFalse)
	{
		gviFreeDevice(device);
		return NULL;
	}

	device->m_methods.m_freeDevice = gviPS2Spu2FreeDevice;
	device->m_methods.m_startDevice = gviPS2Spu2StartDevice;
	device->m_methods.m_stopDevice = gviPS2Spu2StopDevice;
	device->m_methods.m_isDeviceStarted = gviPS2Spu2IsDeviceStarted;
	device->m_methods.m_setDeviceVolume = gviPS2Spu2SetDeviceVolume;
	device->m_methods.m_getDeviceVolume = gviPS2Spu2GetDeviceVolume;
	device->m_methods.m_playPacket = gviPS2Spu2PlayPacket;
	device->m_methods.m_isSourceTalking = gviPS2Spu2IsSourceTalking;
	device->m_methods.m_listTalkingSources = gviPS2Spu2ListTalkingSources;

	GVIPS2Spu2Device = device;

	return device;
}

#endif  
