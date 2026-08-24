#include "gvDevice.h"
#include "gvCodec.h"
#include "gvSource.h"
#include "gvUtil.h"
#include <audioinput.h>

#if !defined(_PSP)
#error This file should only be used with the PSP
#endif

#define GVI_INPUT_LEN 320

#define GVI_CAPTURE_THREAD_STACK_SIZE (1024 * 4)

#define GVI_ADC_ALC    -6 
#define GVI_ADC_GAIN  +30 
#define GVI_ADC_NOIZ  -60 
#define GVI_ADC_HOLD    0 
#define GVI_ADC_DECAY   3 
#define GVI_ADC_ATTACK  2 

typedef struct
{
	GVBool m_capturing;
	GVScalar m_captureVolume;
	GVFrameStamp m_captureClock;
	GVScalar m_captureThreshold;
	GVFrameStamp m_captureLastCrossedThresholdTime;
	GVSample * m_captureBuffer;
	size_t m_captureBufferLen;  
	size_t m_captureBufferWritePos;
	size_t m_captureBufferReadPos;
	SceUID m_captureBufferSemaphore;
	SceUID m_captureThreadID;
	GVBool m_captureThreadStop;
} GVIHardwareData;

static GVIDevice * GVIPSPDevice = NULL;

static void gviHardwareFreeDevice(GVIDevice * device);

static GVBool gviWaitSemaphore(GVIHardwareData * data)
{
	int rcode = sceKernelWaitSema(data->m_captureBufferSemaphore, 1, NULL);
	if(rcode == 0)
		return GVTrue;
	return GVFalse;
}

static GVBool gviSignalSemaphore(GVIHardwareData * data)
{
	int rcode = sceKernelSignalSema(data->m_captureBufferSemaphore, 1);
	if(rcode == 0)
		return GVTrue;
	return GVFalse;
}

GVBool gviHardwareStartup(void)
{
	SceAudioInputParam param;
	int rcode;

	param.alc = GVI_ADC_ALC;
	param.gain = GVI_ADC_GAIN;
	param.noiz = GVI_ADC_NOIZ;
	param.hold = GVI_ADC_HOLD;
	param.decay = GVI_ADC_DECAY;
	param.attack = GVI_ADC_ATTACK;
	rcode = sceAudioInputInitEx(&param);
	if(rcode < 0)
		return GVFalse;

	return GVTrue;
}

void gviHardwareCleanup(void)
{
	if(GVIPSPDevice)
	{
		gviHardwareFreeDevice(GVIPSPDevice);
		GVIPSPDevice = NULL;
	}
}

void gviHardwareThink(void)
{
}

int gviHardwareListDevices(GVDeviceInfo devices[], int maxDevices, GVDeviceType types)
{
	if(maxDevices < 1)
		return 0;

	if(!(types & GV_CAPTURE))
		return 0;

	memset(&devices[0], 0, sizeof(GVDeviceInfo));
	devices[0].m_id = 0;
	strcpy(devices[0].m_name, _T("Headset Mic"));
	devices[0].m_deviceType = GV_CAPTURE;
	devices[0].m_defaultDevice = GV_CAPTURE;
	devices[0].m_hardwareType = GVHardwarePSPHeadset;

	return 1;
}

static void gviHardwareFreeDevice(GVIDevice * device)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;

	GVIPSPDevice = NULL;

	data->m_captureThreadStop = GVTrue;
}

static GVBool gviHardwareStartDevice(GVIDevice * device, GVDeviceType type)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;

	if(type != GV_CAPTURE)
		return GVFalse;

	if(data->m_capturing == GVTrue)
		return GVTrue;

	data->m_captureBufferWritePos = 0;
	data->m_captureBufferReadPos = 0;

	data->m_capturing = GVTrue;

	return GVTrue;
}

static void gviHardwareStopDevice(GVIDevice * device, GVDeviceType type)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;

	if(type != GV_CAPTURE)
		return;

	data->m_capturing = GVFalse;

	data->m_captureClock++;
}

static GVBool gviHardwareIsDeviceStarted(GVIDevice * device, GVDeviceType type)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	if(type != GV_CAPTURE)
		return GVFalse;
	return data->m_capturing;
}

static void gviHardwareSetDeviceVolume(GVIDevice * device, GVDeviceType type, GVScalar volume)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	if(type != GV_CAPTURE)
		return;
	data->m_captureVolume = volume;
}

static GVScalar gviHardwareGetDeviceVolume(GVIDevice * device, GVDeviceType type)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	if(type != GV_CAPTURE)
		return (GVScalar)0;
	return data->m_captureVolume;
}

static void gviHardwareSetCaptureThreshold(GVIDevice * device, GVScalar threshold)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	data->m_captureThreshold = threshold;
}

static GVScalar gviHardwareGetCaptureThreshold(GVIDevice * device)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	return data->m_captureThreshold;
}

static int gviHardwareGetAvailableCaptureBytes(GVDevice device)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	int numSamples;

	if(!data->m_capturing)
		return GVFalse;

	gviWaitSemaphore(data);
		if(data->m_captureBufferWritePos < data->m_captureBufferReadPos)
			numSamples = (data->m_captureBufferLen - data->m_captureBufferReadPos);
		else
			numSamples = (data->m_captureBufferWritePos - data->m_captureBufferReadPos);
	gviSignalSemaphore(data);

	return numSamples;
}

static GVBool gviHardwareCapturePacket(GVDevice device, GVByte * packet, int * len, GVFrameStamp * frameStamp, GVScalar * volume)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	int numBytes;
	int numSamples;
	int numFrames;
	GVBool overThreshold = GVFalse;
	int lenAvailable;
	int framesAvailable;
	int i, j;
	GVSample * readPtr;

	if(!data->m_capturing)
		return GVFalse;

	lenAvailable = *len;

	*len = 0;
	if(volume)
		*volume = 0;

	numSamples = gviHardwareGetAvailableCaptureBytes(device);
	numBytes = (numSamples * GV_BYTES_PER_SAMPLE);

	numFrames = (numBytes / GVIBytesPerFrame);

	if(numFrames == 0)
		return GVFalse;

	framesAvailable = (lenAvailable / GVIEncodedFrameSize);

	numFrames = min(numFrames, framesAvailable);
	if(!numFrames)
		return GVFalse;

	numBytes = (numFrames * GVIBytesPerFrame);
	numSamples = (numBytes / GV_BYTES_PER_SAMPLE);

	readPtr = (data->m_captureBuffer + data->m_captureBufferReadPos);

	if(volume)
		*volume = gviGetSamplesVolume(readPtr, numSamples);

	if(volume)
	{
		overThreshold = (*volume >= data->m_captureThreshold);
	}
	else
	{
		overThreshold = gviIsOverThreshold(readPtr, numSamples, data->m_captureThreshold);
	}

	if(overThreshold)
	{
		data->m_captureLastCrossedThresholdTime = data->m_captureClock;
	}
	else
	{
		overThreshold = ((GVFrameStamp)(data->m_captureClock - data->m_captureLastCrossedThresholdTime) < GVI_HOLD_THRESHOLD_FRAMES);
	}

	if(overThreshold)
	{
		*frameStamp = data->m_captureClock;

		for(i = 0 ; i < numFrames ; i++)
		{
			if(data->m_captureVolume < 1.0)
			{
				for(j = 0 ; j < GVISamplesPerFrame ; j++)
					readPtr[j] = (GVSample)(readPtr[j] * data->m_captureVolume);
			}

			if(device->m_captureFilterCallback)
				device->m_captureFilterCallback(device, readPtr, (GVFrameStamp)(data->m_captureClock + i));

			gviEncode(packet + (GVIEncodedFrameSize * i), readPtr);

			readPtr += GVISamplesPerFrame;
		}
	}

	data->m_captureBufferReadPos += numSamples;
	data->m_captureBufferReadPos %= data->m_captureBufferLen;
	data->m_captureClock += numFrames;

	*len = (numFrames * GVIEncodedFrameSize);

	if(!overThreshold)
		return GVFalse;

	return GVTrue;
}

static int gviPSPCaptureThread(SceSize args, void * argp)
{
	GVIDevice * device = *(GVIDevice **)argp;
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	int rcode;

	assert(device);
	assert(args == sizeof(GVIDevice*));

	while(!data->m_captureThreadStop)
	{
		if(data->m_capturing)
		{
			rcode = sceAudioInputBlocking(GVI_INPUT_LEN, GV_SAMPLES_PER_SECOND,
				data->m_captureBuffer + data->m_captureBufferWritePos);
			if(rcode < 0)
			{
				gviDeviceUnplugged(device);
			}
			else
			{
				gviWaitSemaphore(data);
					data->m_captureBufferWritePos += GVI_INPUT_LEN;
					data->m_captureBufferWritePos %= data->m_captureBufferLen;
				gviSignalSemaphore(data);
			}
		}
		else
		{
			msleep(10);
		}
	}

	if(GVIPSPDevice != NULL)
		sceAudioInputBlocking(GVI_INPUT_LEN, GV_SAMPLES_PER_SECOND, NULL);

	gsifree(data->m_captureBuffer);
	sceKernelDeleteSema(data->m_captureBufferSemaphore);
	gviFreeDevice(device);

	sceKernelExitThread(0);

	return 0;
}

static GVBool gviPSPAudioInitDevice(GVIDevice *device)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	size_t size;
	size_t oldSize;
	int rcode;

	data->m_captureBufferSemaphore = sceKernelCreateSema("capture buffer", SCE_KERNEL_SA_THFIFO, 1, 1, NULL);
	if(data->m_captureBufferSemaphore <= 0)
		return GVFalse;

	size = gviMultiplyByBytesPerMillisecond(GVI_CAPTURE_BUFFER_MILLISECONDS);
	size /= GV_BYTES_PER_SAMPLE; 
	do
	{
		oldSize = size;
		size = gviRoundUpToNearestMultiple(size, GVISamplesPerFrame);
		size = gviRoundUpToNearestMultiple(size, GVI_INPUT_LEN);
	}
	while(size != oldSize);
	data->m_captureBufferLen = size;

	data->m_captureBuffer = (GVSample *)gsimalloc(size * GV_BYTES_PER_SAMPLE);
	if(!data->m_captureBuffer)
	{
		sceKernelDeleteSema(data->m_captureBufferSemaphore);
		return GVFalse;
	}

	data->m_captureThreadID = sceKernelCreateThread("capture",gviPSPCaptureThread,
		SCE_KERNEL_USER_HIGHEST_PRIORITY, GVI_CAPTURE_THREAD_STACK_SIZE, 0, NULL);
	if(data->m_captureThreadID < 0)
	{
		gsifree(data->m_captureBuffer);
		sceKernelDeleteSema(data->m_captureBufferSemaphore);
		return GVFalse;
	}

	rcode = sceKernelStartThread(data->m_captureThreadID, sizeof(GVIDevice*), &device);
	if(rcode < 0)
	{
		sceKernelDeleteThread(data->m_captureThreadID);
		gsifree(data->m_captureBuffer);
		sceKernelDeleteSema(data->m_captureBufferSemaphore);
		return GVFalse;
	}

	return GVTrue;
}

GVDevice gviHardwareNewDevice(GVDeviceID deviceID, GVDeviceType type)
{
	GVIDevice * device;
	GVIHardwareData * data;
	GVBool result;

	if(type != GV_CAPTURE)
		return NULL;

	if(GVIPSPDevice != NULL)
		return NULL;

	device = gviNewDevice(deviceID, GVHardwarePSPHeadset, type, sizeof(GVIHardwareData));
	if(!device)
		return NULL;

	result = gviPSPAudioInitDevice(device);
	if(result == GVFalse)
	{
		gviFreeDevice(device);
		return NULL;
	}

	device->m_methods.m_freeDevice = gviHardwareFreeDevice;
	device->m_methods.m_startDevice = gviHardwareStartDevice;
	device->m_methods.m_stopDevice = gviHardwareStopDevice;
	device->m_methods.m_isDeviceStarted = gviHardwareIsDeviceStarted;
	device->m_methods.m_setDeviceVolume = gviHardwareSetDeviceVolume;
	device->m_methods.m_getDeviceVolume = gviHardwareGetDeviceVolume;
	device->m_methods.m_setCaptureThreshold = gviHardwareSetCaptureThreshold;
	device->m_methods.m_getCaptureThreshold = gviHardwareGetCaptureThreshold;
	device->m_methods.m_getAvailableCaptureBytes = gviHardwareGetAvailableCaptureBytes;
	device->m_methods.m_capturePacket = gviHardwareCapturePacket;

	data = (GVIHardwareData *)device->m_data;

	data->m_captureVolume = (GVScalar)1.0;
	data->m_captureClock = 0;
	data->m_captureLastCrossedThresholdTime = (data->m_captureClock - GVI_HOLD_THRESHOLD_FRAMES - 1);

	GVIPSPDevice = device;

	return device;
}
