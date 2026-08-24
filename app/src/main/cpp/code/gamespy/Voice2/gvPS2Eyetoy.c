/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#include "gvPS2Eyetoy.h"
#if !defined(GV_NO_PS2_EYETOY)
#include "gvDevice.h"
#include "gvCodec.h"
#include "gvSource.h"
#include "gvUtil.h"
#include <liblgvid.h>

#if !defined(_PS2)
#error This file should only be used with the PlayStation2
#endif

typedef struct
{
	int m_handle;

	GVBool m_capturing;
	GVFrameStamp m_captureClock;
	GVScalar m_captureThreshold;
	GVFrameStamp m_captureLastCrossedThresholdTime;
	GVSample * m_captureBuffer;
	int m_captureBufferBytes;
} GVIPS2EyetoyData;

static GVIDeviceList GVIDevices;
static GVBool GVILGVidInitialized;

static void gviFreeArrayDevice(void * elem)
{
	GVIDevice * device = *(GVIDevice **)elem;
	GVIPS2EyetoyData * data = (GVIPS2EyetoyData *)device->m_data;

	lgVidClose(data->m_handle);

	gsifree(data->m_captureBuffer);

	gviFreeDevice(device);
}

GVBool gviPS2EyetoyStartup(void)
{
	int result;

	GVIDevices = gviNewDeviceList(gviFreeArrayDevice);
	if(!GVIDevices)
		return GVFalse;

	if(GVILGVidInitialized)
	{
		result = lgVidReInit();
	}
	else
	{
		lgVidCustomAllocator anAllocator;
		anAllocator.pfnMalloc = gsimalloc;
		anAllocator.pfnMemAlign = gsimemalign;
		anAllocator.pfnFree = gsifree;
#ifdef GVI_LGVID_OLD_DRIVER
		result = lgVidInit(NULL);
#else
		result = lgVidInit(&anAllocator, NULL, 0);
#endif

	}
	if(LGVID_FAILED(result))
	{
		gviFreeDeviceList(GVIDevices);
		GVIDevices = NULL;
		return GVFalse;
	}

	GVILGVidInitialized = GVTrue;

	return GVTrue;
}

void gviPS2EyetoyCleanup(void)
{
	if(GVIDevices)
	{
		gviFreeDeviceList(GVIDevices);
		GVIDevices = NULL;
	}

	lgVidUnloadIOPModule();
}

int gviPS2EyetoyListDevices(GVDeviceInfo devices[], int maxDevices, GVDeviceType types)
{
	lgVidDeviceDesc eyetoyDesc;
	GVDeviceType supportedTypes;
	int result;
	int index;
	int numDevices = 0;
	int modeIndex;

	if(!(types & GV_CAPTURE))
		return 0;

enumerate:
	for(index = 0 ; numDevices < maxDevices ; index++)
	{
		result = lgVidEnumerate(index, &eyetoyDesc);

		if(result == LGVID_ERR_DEVICE_LOST)
		{
			numDevices = 0;
			goto enumerate;
		}

		if(LGVID_FAILED(result))
			break;

		supportedTypes = (GVDeviceType)0;
		for(modeIndex = 0 ; modeIndex < eyetoyDesc.SupportedModesCount ; modeIndex++)
		{
			if(eyetoyDesc.SupportedModes[modeIndex].OperatingMode.AudioRate == LGVID_AUD_8000)
			{
				supportedTypes = GV_CAPTURE;
				break;
			}
		}

		if(supportedTypes == GV_CAPTURE)
		{
#if defined(GSI_UNICODE)
			char name[GV_DEVICE_NAME_LEN];
#endif

			devices[numDevices].m_id = (int)(index | GVI_EYETOY_DEVICEID_BIT);
#if defined(GSI_UNICODE)
			strncpy(name, eyetoyDesc.FriendlyName, GV_DEVICE_NAME_LEN);
			name[GV_DEVICE_NAME_LEN - 1] = '\0';
			AsciiToUCS2String(name, devices[numDevices].m_name);
#else
			strncpy(devices[numDevices].m_name, eyetoyDesc.FriendlyName, GV_DEVICE_NAME_LEN);
			devices[numDevices].m_name[GV_DEVICE_NAME_LEN - 1] = '\0';
#endif
			devices[numDevices].m_deviceType = supportedTypes;
			devices[numDevices].m_defaultDevice = (GVDeviceType)0; 
			devices[numDevices].m_hardwareType = GVHardwarePS2Eyetoy;

			numDevices++;
		}
	}

	return numDevices;
}

static void gviPS2EyetoyFreeDevice(GVIDevice * device)
{
	gviDeleteDeviceFromList(GVIDevices, device);
}

static GVBool gviPS2EyetoyStartDevice(GVIDevice * device, GVDeviceType type)
{
	GVIPS2EyetoyData * data = (GVIPS2EyetoyData *)device->m_data;
	int result;

	if(type != GV_CAPTURE)
		return GVFalse;

	result = lgVidStartStreaming(data->m_handle);
	if(LGVID_FAILED(result))
		return GVFalse;

	data->m_captureBufferBytes = 0;

	data->m_capturing = GVTrue;

	return GVTrue;
}

static void gviPS2EyetoyStopDevice(GVIDevice * device, GVDeviceType type)
{
	GVIPS2EyetoyData * data = (GVIPS2EyetoyData *)device->m_data;

	if(!(type & GV_CAPTURE))
		return;

	lgVidStopStreaming(data->m_handle);

	data->m_capturing = GVFalse;

	data->m_captureClock++;
}

static GVBool gviPS2EyetoyIsDeviceStarted(GVIDevice * device, GVDeviceType type)
{
	GVIPS2EyetoyData * data = (GVIPS2EyetoyData *)device->m_data;

	if(!(type & GV_CAPTURE))
		return GVFalse;

	return data->m_capturing;
}

static void gviPS2EyetoySetDeviceVolume(GVIDevice * device, GVDeviceType type, GVScalar volume)
{
	GVIPS2EyetoyData * data = (GVIPS2EyetoyData *)device->m_data;
	lgVidCameraControl cameraControl;
	u_char gain;

	if(!(type & GV_CAPTURE))
		return;

	gain = (u_char)(volume * 8);
	gain = (unsigned char)min(gain, 8);

	cameraControl.Flags = LGVID_FLAG_AUDIO_GAIN;
	cameraControl.AudioGain = gain;

	lgVidSetCameraControl(data->m_handle, &cameraControl);
}

static GVScalar gviPS2EyetoyGetDeviceVolume(GVIDevice * device, GVDeviceType type)
{
	GVIPS2EyetoyData * data = (GVIPS2EyetoyData *)device->m_data;
	lgVidCameraControl cameraControl;
	int result;

	if(!(type & GV_CAPTURE))
		return 0;

	cameraControl.Flags = LGVID_FLAG_AUDIO_GAIN;
	result = lgVidGetCameraControl(data->m_handle, &cameraControl);
	if(LGVID_FAILED(result) || !(cameraControl.Flags & LGVID_FLAG_AUDIO_GAIN))
		return 1.0;

	return (GVScalar)(cameraControl.AudioGain / 8.0);
}

static void gviPS2EyetoySetCaptureThreshold(GVIDevice * device, GVScalar threshold)
{
	GVIPS2EyetoyData * data = (GVIPS2EyetoyData *)device->m_data;
	data->m_captureThreshold = threshold;
}

static GVScalar gviPS2EyetoyGetCaptureThreshold(GVIDevice * device)
{
	GVIPS2EyetoyData * data = (GVIPS2EyetoyData *)device->m_data;
	return data->m_captureThreshold;
}

static int gviPS2EyetoyGetAvailableCaptureBytes(GVDevice device)
{
	GVIPS2EyetoyData * data = (GVIPS2EyetoyData *)device->m_data;

	if(!data->m_capturing)
		return 0;

	return 1;
}

static void gviProcessCapturedFrame(GVDevice device, GVByte * frameOut, GVSample * frameIn, GVScalar * volume, GVBool * threshold)
{
	GVIPS2EyetoyData * data = (GVIPS2EyetoyData *)device->m_data;
	GVScalar frameVolume;

	if(volume)
	{
		frameVolume = gviGetSamplesVolume(frameIn, GVISamplesPerFrame);
		if(frameVolume > *volume)
			*volume = frameVolume;
	}

	if(threshold && !*threshold)
	{
		if(volume)
		{
			*threshold = (*volume >= data->m_captureThreshold);
		}
		else
		{
			*threshold = gviIsOverThreshold(frameIn, GVISamplesPerFrame, data->m_captureThreshold);
		}
	}

	if(device->m_captureFilterCallback)
		device->m_captureFilterCallback(device, frameIn, data->m_captureClock);

	data->m_captureClock++;

	gviEncode(frameOut, frameIn);
}

static GVBool gviPS2EyetoyCapturePacket(GVDevice device, GVByte * packet, int * len, GVFrameStamp * frameStamp, GVScalar * volume)
{
	GVIPS2EyetoyData * data = (GVIPS2EyetoyData *)device->m_data;
	lgVidAudioDesc audioDesc;
	GVBool overThreshold;
	int result;
	int numFrames;
	int lenAvailable;
	int framesAvailable;
	int framesCaptured;
	GVSample * frameIn;
	GVByte * frameOut;

	lenAvailable = *len;

	*len = 0;
	if(volume)
		*volume = 0;

	if(!data->m_capturing)
		return GVFalse;

	*frameStamp = data->m_captureClock;

	framesAvailable = (lenAvailable / GVIEncodedFrameSize);

	framesCaptured = (data->m_captureBufferBytes / GVIBytesPerFrame);

	overThreshold = GVFalse;
	frameIn = data->m_captureBuffer;
	frameOut = packet;
	for(numFrames = 0 ; (numFrames < framesAvailable) && (numFrames < framesCaptured) ; numFrames++)
	{
		gviProcessCapturedFrame(device, frameOut, frameIn, volume, &overThreshold);

		frameIn += GVISamplesPerFrame;
		frameOut += GVIEncodedFrameSize;
	}

	if(numFrames)
	{
		data->m_captureBufferBytes -= (numFrames * GVIBytesPerFrame);
		if(data->m_captureBufferBytes)
			memmove(data->m_captureBuffer, data->m_captureBuffer + (numFrames * GVIBytesPerFrame), (unsigned int)data->m_captureBufferBytes);
	}

	if(numFrames < framesAvailable)
	{
		assert(data->m_captureBufferBytes < GVIBytesPerFrame);

		audioDesc.Samples = (((u_char *)data->m_captureBuffer) + data->m_captureBufferBytes);

		result = lgVidReadAudio(data->m_handle, &audioDesc);
		if(LGVID_FAILED(result))
		{
			gviDeviceUnplugged(device);
			return GVFalse;
		}

		if(audioDesc.TimeStamp != -1)
		{
			data->m_captureBufferBytes += audioDesc.TotalBytes;

			frameIn = data->m_captureBuffer;

			for( ; numFrames < framesAvailable ; numFrames++)
			{
				if(data->m_captureBufferBytes < GVIBytesPerFrame)
					break;

				gviProcessCapturedFrame(device, frameOut, frameIn, volume, &overThreshold);

				data->m_captureBufferBytes -= GVIBytesPerFrame;

				frameIn += GVISamplesPerFrame;
				frameOut += GVIEncodedFrameSize;
			}
			
			if(data->m_captureBufferBytes)
				memmove(data->m_captureBuffer, frameIn, (unsigned)data->m_captureBufferBytes);
		}
	}

	if(overThreshold)
	{
		data->m_captureLastCrossedThresholdTime = data->m_captureClock;
	}
	else
	{
		overThreshold = ((GVFrameStamp)(*frameStamp - data->m_captureLastCrossedThresholdTime) < GVI_HOLD_THRESHOLD_FRAMES);
	}

	*len = (numFrames * GVIEncodedFrameSize);

	if(!overThreshold || (*len == 0))
		return GVFalse;

	return GVTrue;
}

static GVBool gviPS2EyetoyInitDevice(GVIDevice * device, int deviceIndex, GVDeviceType type)
{
	lgVidDeviceDesc eyetoyDesc;
	lgVidOpenParam openParam;
	GVIPS2EyetoyData * data;
	int result;
	int size;
	int i;

	if(type != GV_CAPTURE)
		return GVFalse;

	result = lgVidEnumerate(deviceIndex, &eyetoyDesc);
	if(LGVID_FAILED(result))
		return GVFalse;

	for(i = 0 ; i < eyetoyDesc.SupportedModesCount ; i++)
	{
		if(eyetoyDesc.SupportedModes[i].OperatingMode.AudioRate == LGVID_AUD_8000)
			break;
	}
	if(i == eyetoyDesc.SupportedModesCount)
		return GVFalse;

	memcpy(&openParam.OperatingMode, &eyetoyDesc.SupportedModes[i].OperatingMode, sizeof(lgVidOperatingMode));
	openParam.AudioBufferDuration = GVI_CAPTURE_BUFFER_MILLISECONDS;

	data = (GVIPS2EyetoyData *)device->m_data;

	result = lgVidOpen(deviceIndex, &openParam, &data->m_handle);
	if(LGVID_FAILED(result))
		return GVFalse;

	data->m_captureLastCrossedThresholdTime = (GVFrameStamp)(data->m_captureClock - GVI_HOLD_THRESHOLD_FRAMES - 1);

	size = (GVI_CAPTURE_BUFFER_MILLISECONDS * GV_BYTES_PER_SECOND / 1000);
	size += GVIBytesPerFrame;
	data->m_captureBuffer = (GVSample *)gsimalloc((unsigned)size);
	if(!data->m_captureBuffer)
	{
		lgVidClose(data->m_handle);
		return GVFalse;
	}

	return GVTrue;
}

GVDevice gviPS2EyetoyNewDevice(GVDeviceID deviceID, GVDeviceType type)
{
	GVIDevice * device;
	GVBool result;

	if(type != GV_CAPTURE)
		return NULL;

	if(!(deviceID & GVI_EYETOY_DEVICEID_BIT))
		return NULL;

	device = gviNewDevice(deviceID, GVHardwarePS2Eyetoy, type, sizeof(GVIPS2EyetoyData));
	if(!device)
		return NULL;

	result = gviPS2EyetoyInitDevice(device, (int)(deviceID & ~GVI_EYETOY_DEVICEID_BIT), type);
	if(result == GVFalse)
	{
		gviFreeDevice(device);
		return NULL;
	}

	device->m_methods.m_freeDevice = gviPS2EyetoyFreeDevice;
	device->m_methods.m_startDevice = gviPS2EyetoyStartDevice;
	device->m_methods.m_stopDevice = gviPS2EyetoyStopDevice;
	device->m_methods.m_isDeviceStarted = gviPS2EyetoyIsDeviceStarted;
	device->m_methods.m_setDeviceVolume = gviPS2EyetoySetDeviceVolume;
	device->m_methods.m_getDeviceVolume = gviPS2EyetoyGetDeviceVolume;
	device->m_methods.m_setCaptureThreshold = gviPS2EyetoySetCaptureThreshold;
	device->m_methods.m_getCaptureThreshold = gviPS2EyetoyGetCaptureThreshold;
	device->m_methods.m_getAvailableCaptureBytes = gviPS2EyetoyGetAvailableCaptureBytes;
	device->m_methods.m_capturePacket = gviPS2EyetoyCapturePacket;

	gviAppendDeviceToList(GVIDevices, device);

	return device;
}

#endif 
