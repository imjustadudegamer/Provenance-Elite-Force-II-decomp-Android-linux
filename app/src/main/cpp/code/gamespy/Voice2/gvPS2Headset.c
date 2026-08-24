#include "gvPS2Headset.h"
#if !defined(GV_NO_PS2_HEADSET)
#include "gvDevice.h"
#include "gvCodec.h"
#include "gvSource.h"
#include "gvUtil.h"
#include <liblgaud.h>

#if !defined(_PS2)
#error This file should only be used with the PlayStation2
#endif

#define GVI_PLAYBACK_STAYAHEAD_MILLISECONDS  50

typedef struct
{
	int m_handle;

	GVBool m_playing;
	GVScalar m_playbackVolume;
	GVFrameStamp m_playbackClock;
	GVISourceList m_playbackSources;
	GVSample * m_playbackBuffer;

	GVBool m_capturing;
	GVScalar m_captureVolume;
	GVFrameStamp m_captureClock;
	GVScalar m_captureThreshold;
	GVFrameStamp m_captureLastCrossedThresholdTime;
	GVSample * m_captureBuffer;
	int m_captureBufferBytes;
} GVIPS2HeadsetData;

static GVIDeviceList GVIDevices;

static GVIDevice * gviFindDeviceByID(GVDeviceID deviceID)
{
	GVIDevice * device;
	int num;
	int i;

	num = gviGetNumDevices(GVIDevices);
	for(i = 0 ; i < num ; i++)
	{
		device = gviGetDevice(GVIDevices, i);
		if(device->m_deviceID == deviceID)
			return device;
	}

	return NULL;
}

static void gviFreeArrayDevice(void * elem)
{
	GVIDevice * device = *(GVIDevice **)elem;
	GVIPS2HeadsetData * data = (GVIPS2HeadsetData *)device->m_data;

	lgAudClose(data->m_handle);

	if(device->m_types & GV_PLAYBACK)
	{
		if(data->m_playbackSources)
			gviFreeSourceList(data->m_playbackSources);
		gsifree(data->m_playbackBuffer);
	}

	if(device->m_types & GV_CAPTURE)
	{
		gsifree(data->m_captureBuffer);
	}

	gviFreeDevice(device);
}

GVBool gviPS2HeadsetStartup(void)
{
	int result;

	GVIDevices = gviNewDeviceList(gviFreeArrayDevice);
	if(!GVIDevices)
		return GVFalse;

	result = lgAudInit(gsimalloc, gsifree);
	if(LGAUD_FAILED(result))
	{
		gviFreeDeviceList(GVIDevices);
		GVIDevices = NULL;
		return GVFalse;
	}

	return GVTrue;
}

void gviPS2HeadsetCleanup(void)
{
	if(GVIDevices)
	{
		gviFreeDeviceList(GVIDevices);
		GVIDevices = NULL;
	}

	lgAudDeInit();
}

static GVBool gviPlaybackDeviceThink(GVIDevice * device)
{
	GVIPS2HeadsetData * data = (GVIPS2HeadsetData *)device->m_data;
	int result;
	int remainingBytes;
	int numFrames;
	int newBytes;
	GVBool wroteToBuffer;
	int size;
	int i;

	if(!data->m_playing)
		return GVTrue;

	result = lgAudGetRemainingPlaybackBytes(data->m_handle, &remainingBytes);
	if(LGAUD_FAILED(result))
		return GVFalse;

	newBytes = (gviMultiplyByBytesPerMillisecond(GVI_PLAYBACK_STAYAHEAD_MILLISECONDS) - remainingBytes);
	newBytes = gviRoundUpToNearestMultiple(newBytes, GVIBytesPerFrame);

	numFrames = (newBytes / GVIBytesPerFrame);

	for(i = 0 ; i < numFrames ; i++)
	{
		wroteToBuffer = gviWriteSourcesToBuffer(data->m_playbackSources, data->m_playbackClock, data->m_playbackBuffer, 1);

		if(!wroteToBuffer)
			memset(data->m_playbackBuffer, 0, (unsigned int)GVIBytesPerFrame);

		if(device->m_playbackFilterCallback)
			device->m_playbackFilterCallback(device, data->m_playbackBuffer, data->m_playbackClock);

		size = GVIBytesPerFrame;
		result = lgAudWrite(data->m_handle, LGAUD_BLOCKMODE_BLOCKING, (u_char *)data->m_playbackBuffer, &size);
		if(LGAUD_FAILED(result))
			return GVFalse;

		data->m_playbackClock++;
	}

	return GVTrue;
}

void gviPS2HeadsetThink(void)
{
	GVIDevice * device;
	GVBool rcode;
	int num;
	int i;

	if(!GVIDevices)
		return;

	num = gviGetNumDevices(GVIDevices);
	for(i = (num - 1) ; i >= 0 ; i--)
	{
		device = gviGetDevice(GVIDevices, i);

		if(device->m_types & GV_PLAYBACK)
		{
			rcode = gviPlaybackDeviceThink(device);

			if(!rcode)
				gviDeviceUnplugged(device);
		}
	}
}

static GVBool gviDoesSupportOurFormat(lgAudSamplingFormat * formats, int count)
{
	int i;

	for(i = 0 ; i < count ; i++)
	{
		if(formats[i].Channels == 1)
			if(formats[i].BitResolution == 16)
				if(formats[i].LowerSamplingRate <= GV_SAMPLES_PER_SECOND)
					if(formats[i].HigherSamplingRate >= GV_SAMPLES_PER_SECOND)
						return GVTrue;
	}

	return GVFalse;
}

int gviPS2HeadsetListDevices(GVDeviceInfo devices[], int maxDevices, GVDeviceType types)
{
	lgAudDeviceDesc headsetDesc;
	GVDeviceType supportedTypes;
	int result;
	int index;
	int numDevices = 0;

enumerate:
	for(index = 0 ; numDevices < maxDevices ; index++)
	{
		result = lgAudEnumerate(index, &headsetDesc);

		if(result == LGAUD_ERR_DEVICE_LOST)
		{
			numDevices = 0;
			goto enumerate;
		}

		if(LGAUD_FAILED(result))
			break;

		supportedTypes = (GVDeviceType)0;
		if(gviDoesSupportOurFormat(headsetDesc.RecordingFormats, headsetDesc.RecordingFormatsCount))
			supportedTypes |= GV_CAPTURE;
		if(gviDoesSupportOurFormat(headsetDesc.PlaybackFormats, headsetDesc.PlaybackFormatsCount))
			supportedTypes |= GV_PLAYBACK;

		if(supportedTypes & types)
		{
			devices[numDevices].m_id = index;
			devices[numDevices].m_deviceType = supportedTypes;
			devices[numDevices].m_defaultDevice = (GVDeviceType)0; 

			if(supportedTypes == GV_CAPTURE)
			{
				_tcscpy(devices[numDevices].m_name, _T("USB Microphone"));
				devices[numDevices].m_hardwareType = GVHardwarePS2Microphone;
			}
			else if(supportedTypes == GV_PLAYBACK)
			{
				_tcscpy(devices[numDevices].m_name, _T("USB Speakers"));
				devices[numDevices].m_hardwareType = GVHardwarePS2Speakers;
			}
			else
			{
				_tcscpy(devices[numDevices].m_name, _T("USB Headset"));
				devices[numDevices].m_hardwareType = GVHardwarePS2Headset;
			}

			numDevices++;
		}
	}

	return numDevices;
}

static void gviPS2HeadsetFreeDevice(GVIDevice * device)
{
	gviDeleteDeviceFromList(GVIDevices, device);
}

static GVBool gviStartPlaybackDevice(GVIPS2HeadsetData * data)
{
	int result;

	result = lgAudStartPlayback(data->m_handle);
	if(LGAUD_FAILED(result))
		return GVFalse;

	data->m_playbackClock = 0;

	data->m_playing = GVTrue;

	return GVTrue;
}

static GVBool gviStartCaptureDevice(GVIPS2HeadsetData * data)
{
	int result;

	result = lgAudStartRecording(data->m_handle);
	if(LGAUD_FAILED(result))
		return GVFalse;

	data->m_captureBufferBytes = 0;

	data->m_capturing = GVTrue;

	return GVTrue;
}

static GVBool gviPS2HeadsetStartDevice(GVIDevice * device, GVDeviceType type)
{
	GVIPS2HeadsetData * data = (GVIPS2HeadsetData *)device->m_data;

	if(type == GV_PLAYBACK)
		return gviStartPlaybackDevice(data);
	if(type == GV_CAPTURE)
		return gviStartCaptureDevice(data);
	if(type == GV_CAPTURE_AND_PLAYBACK)
	{
		if(!gviStartPlaybackDevice(data))
			return GVFalse;
		if(!gviStartCaptureDevice(data))
		{
			device->m_methods.m_stopDevice(device, GV_PLAYBACK);
			return GVFalse;
		}
		return GVTrue;
	}
	return GVFalse;
}

static void gviPS2HeadsetStopDevice(GVIDevice * device, GVDeviceType type)
{
	GVIPS2HeadsetData * data = (GVIPS2HeadsetData *)device->m_data;

	if(type & GV_PLAYBACK)
	{
		lgAudStopPlayback(data->m_handle);

		data->m_playing = GVFalse;

		gviClearSourceList(data->m_playbackSources);
	}
	if(type & GV_CAPTURE)
	{
		lgAudStopRecording(data->m_handle);

		data->m_capturing = GVFalse;

		data->m_captureClock++;
	}
}

static GVBool gviPS2HeadsetIsDeviceStarted(GVIDevice * device, GVDeviceType type)
{
	GVIPS2HeadsetData * data = (GVIPS2HeadsetData *)device->m_data;

	if(type == GV_PLAYBACK)
		return data->m_playing;
	if(type == GV_CAPTURE)
		return data->m_capturing;
	if(type == GV_CAPTURE_AND_PLAYBACK)
		return (data->m_playing && data->m_capturing);
	return GVFalse;
}

static void gviPS2HeadsetSetDeviceVolume(GVIDevice * device, GVDeviceType type, GVScalar volume)
{
	GVIPS2HeadsetData * data = (GVIPS2HeadsetData *)device->m_data;

	if(type & GV_PLAYBACK)
	{
		lgAudSetPlaybackVolume(data->m_handle, LGAUD_CH_BOTH, (u_char)(volume * 100));
		data->m_playbackVolume = volume;
	}
	if(type & GV_CAPTURE)
	{
		lgAudSetRecordingVolume(data->m_handle, LGAUD_CH_BOTH, (u_char)(volume * 100));
		data->m_captureVolume = volume;
	}
}

static GVScalar gviPS2HeadsetGetDeviceVolume(GVIDevice * device, GVDeviceType type)
{
	GVIPS2HeadsetData * data = (GVIPS2HeadsetData *)device->m_data;

	if(type & GV_PLAYBACK)
		return data->m_playbackVolume;
	return data->m_captureVolume;
}

static void gviPS2HeadsetSetCaptureThreshold(GVIDevice * device, GVScalar threshold)
{
	GVIPS2HeadsetData * data = (GVIPS2HeadsetData *)device->m_data;
	data->m_captureThreshold = threshold;
}

static GVScalar gviPS2HeadsetGetCaptureThreshold(GVIDevice * device)
{
	GVIPS2HeadsetData * data = (GVIPS2HeadsetData *)device->m_data;
	return data->m_captureThreshold;
}

static int gviPS2HeadsetGetAvailableCaptureBytes(GVDevice device)
{
	GVIPS2HeadsetData * data = (GVIPS2HeadsetData *)device->m_data;
	int result;
	int newBytes;
	int numFrames;

	if(!data->m_capturing)
		return 0;

	result = lgAudGetAvailableRecordingBytes(data->m_handle, &newBytes);
	if(LGAUD_FAILED(result))
	{
		gviDeviceUnplugged(device);
		return 0;
	}

	numFrames = (newBytes / GVIBytesPerFrame);

	newBytes = (numFrames * GVIEncodedFrameSize);

	return newBytes;
}

static void gviProcessCapturedFrame(GVDevice device, GVByte * frameOut, GVSample * frameIn, GVScalar * volume, GVBool * threshold)
{
	GVIPS2HeadsetData * data = (GVIPS2HeadsetData *)device->m_data;
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

static GVBool gviPS2HeadsetCapturePacket(GVDevice device, GVByte * packet, int * len, GVFrameStamp * frameStamp, GVScalar * volume)
{
	GVIPS2HeadsetData * data = (GVIPS2HeadsetData *)device->m_data;
	GVBool overThreshold;
	int result;
	int numFrames;
	int readSize;
	int lenAvailable;
	int framesAvailable;
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

	overThreshold = GVFalse;
	frameIn = data->m_captureBuffer;
	frameOut = packet;
	for(numFrames = 0 ; numFrames < framesAvailable ; numFrames++)
	{
		readSize = (GVIBytesPerFrame - data->m_captureBufferBytes);
		result = lgAudRead(data->m_handle, LGAUD_BLOCKMODE_NOT_BLOCKING, ((u_char *)data->m_captureBuffer) + data->m_captureBufferBytes, &readSize);
		if(LGAUD_FAILED(result))
		{
			gviDeviceUnplugged(device);
			return GVFalse;
		}

		if(readSize != (GVIBytesPerFrame - data->m_captureBufferBytes))
		{
			data->m_captureBufferBytes += readSize;
			break;
		}

		gviProcessCapturedFrame(device, frameOut, frameIn, volume, &overThreshold);

		data->m_captureBufferBytes = 0;

		frameOut += GVIEncodedFrameSize;
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

static void gviPS2HeadsetPlayPacket(GVIDevice * device, const GVByte * packet, int len, GVSource source, GVFrameStamp frameStamp, GVBool mute)
{
	GVIPS2HeadsetData * data = (GVIPS2HeadsetData *)device->m_data;

	if(!data->m_playing)
		return;

	gviAddPacketToSourceList(data->m_playbackSources, packet, len, source, frameStamp, mute, data->m_playbackClock);
}

static GVBool gviPS2HeadsetIsSourceTalking(GVDevice device, GVSource source)
{
	GVIPS2HeadsetData * data = (GVIPS2HeadsetData *)device->m_data;

	if(!data->m_playing)
		return GVFalse;

	return gviIsSourceTalking(data->m_playbackSources, source);
}

static int gviPS2HeadsetListTalkingSources(GVDevice device, GVSource sources[], int maxSources)
{
	GVIPS2HeadsetData * data = (GVIPS2HeadsetData *)device->m_data;

	if(!data->m_playing)
		return GVFalse;

	return gviListTalkingSources(data->m_playbackSources, sources, maxSources);
}

static GVBool gviPS2HeadsetInitDevice(GVIDevice * device, int deviceIndex, GVDeviceType type)
{
	GVIPS2HeadsetData * data;
	lgAudOpenParam openParam;
	int result;

	openParam.Mode = 0;
	if(type & GV_CAPTURE)
	{
		openParam.Mode |= LGAUD_MODE_RECORDING;
		openParam.RecordingFormat.Channels = 1;
		openParam.RecordingFormat.BitResolution = 16;
		openParam.RecordingFormat.SamplingRate = GV_SAMPLES_PER_SECOND;
		openParam.RecordingFormat.BufferMilliseconds = GVI_CAPTURE_BUFFER_MILLISECONDS;
	}
	if(type & GV_PLAYBACK)
	{
		openParam.Mode |= LGAUD_MODE_PLAYBACK;
		openParam.PlaybackFormat.Channels = 1;
		openParam.PlaybackFormat.BitResolution = 16;
		openParam.PlaybackFormat.SamplingRate = GV_SAMPLES_PER_SECOND;
		openParam.PlaybackFormat.BufferMilliseconds = GVI_PLAYBACK_BUFFER_MILLISECONDS;
	}

	data = (GVIPS2HeadsetData *)device->m_data;

	result = lgAudOpen(deviceIndex, &openParam, &data->m_handle);
	if(LGAUD_FAILED(result))
		return GVFalse;

	if(type & GV_PLAYBACK)
	{
		data->m_playbackSources = gviNewSourceList();
		if(!data->m_playbackSources)
		{
			lgAudClose(data->m_handle);
			return GVFalse;
		}

		data->m_playbackBuffer = (GVSample *)gsimalloc((unsigned int)GVIBytesPerFrame);
		if(!data->m_playbackBuffer)
		{
			gviFreeSourceList(data->m_playbackSources);
			lgAudClose(data->m_handle);
			return GVFalse;
		}

		data->m_playbackVolume = 1.0;
	}

	if(type & GV_CAPTURE)
	{
		data->m_captureVolume = 1.0;
		data->m_captureLastCrossedThresholdTime = (GVFrameStamp)(data->m_captureClock - GVI_HOLD_THRESHOLD_FRAMES - 1);

		data->m_captureBuffer = (GVSample *)gsimalloc((unsigned int)GVIBytesPerFrame);
		if(!data->m_captureBuffer)
		{
			if(type & GV_PLAYBACK)
			{
				gviFreeSourceList(data->m_playbackSources);
				gsifree(data->m_playbackBuffer);
			}
			lgAudClose(data->m_handle);
			return GVFalse;
		}
	}

	return GVTrue;
}

GVDevice gviPS2HeadsetNewDevice(GVDeviceID deviceID, GVDeviceType type)
{
	GVIDevice * device;
	GVBool result;

	if(!(type & GV_CAPTURE_AND_PLAYBACK))
		return NULL;

	if(gviFindDeviceByID(deviceID))
		return NULL;

	device = gviNewDevice(deviceID, GVHardwarePS2Headset, type, sizeof(GVIPS2HeadsetData));
	if(!device)
		return NULL;

	result = gviPS2HeadsetInitDevice(device, deviceID, type);
	if(result == GVFalse)
	{
		gviFreeDevice(device);
		return NULL;
	}

	device->m_methods.m_freeDevice = gviPS2HeadsetFreeDevice;
	device->m_methods.m_startDevice = gviPS2HeadsetStartDevice;
	device->m_methods.m_stopDevice = gviPS2HeadsetStopDevice;
	device->m_methods.m_isDeviceStarted = gviPS2HeadsetIsDeviceStarted;
	device->m_methods.m_setDeviceVolume = gviPS2HeadsetSetDeviceVolume;
	device->m_methods.m_getDeviceVolume = gviPS2HeadsetGetDeviceVolume;
	device->m_methods.m_setCaptureThreshold = gviPS2HeadsetSetCaptureThreshold;
	device->m_methods.m_getCaptureThreshold = gviPS2HeadsetGetCaptureThreshold;
	device->m_methods.m_getAvailableCaptureBytes = gviPS2HeadsetGetAvailableCaptureBytes;
	device->m_methods.m_capturePacket = gviPS2HeadsetCapturePacket;
	device->m_methods.m_playPacket = gviPS2HeadsetPlayPacket;
	device->m_methods.m_isSourceTalking = gviPS2HeadsetIsSourceTalking;
	device->m_methods.m_listTalkingSources = gviPS2HeadsetListTalkingSources;

	gviAppendDeviceToList(GVIDevices, device);

	return device;
}

#endif 
