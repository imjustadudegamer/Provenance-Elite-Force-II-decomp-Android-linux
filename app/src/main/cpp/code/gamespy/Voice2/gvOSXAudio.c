#include "gvOSXAudio.h"
#include "gvDevice.h"
#include "gvCodec.h"
#include "gvSource.h"
#include "gvUtil.h"
#include <AudioToolbox/AudioConverter.h>
#include <CoreAudio/CoreAudio.h>
#include <pthread.h>

#if !defined(_MACOSX)
#error This file should only be used with MacOS X
#endif

#if !defined(GVI_VOLUME_IN_SOFTWARE)
	#define GVI_VOLUME_IN_SOFTWARE 1
#endif

typedef struct GVICapturedFrame
{
	GVFrameStamp m_frameStamp;
	struct GVICapturedFrame * m_next;
	GVSample m_frame[1];
} GVICapturedFrame;

typedef struct
{
	GVBool m_playing;
	AudioConverterRef m_playbackConverter;
	AudioStreamBasicDescription m_playbackStreamDescriptor;
	GVFrameStamp m_playbackClock;
	GVSample * m_playbackBuffer;
	GVISourceList m_playbackSources;
	int m_playbackChannel;
#if GVI_VOLUME_IN_SOFTWARE
	GVScalar m_playbackVolume;
#endif

	GVBool m_capturing;
	AudioConverterRef m_captureConverter;
	AudioStreamBasicDescription m_captureStreamDescriptor;
	GVFrameStamp m_captureClock;
	GVSample * m_captureBuffer;
	GVICapturedFrame m_capturedFrames;
	GVICapturedFrame m_captureAvailableFrames;
	GVScalar m_captureThreshold;
	GVFrameStamp m_captureLastCrossedThresholdTime;
	int m_captureChannel;
#if GVI_VOLUME_IN_SOFTWARE
	GVScalar m_captureVolume;
#endif

	pthread_mutex_t m_mutex;
	GVBool m_unplugged;
} GVIHardwareData;

typedef struct
{
	GVIDevice * m_device;
	AudioBufferList * m_capturedAudio;
	GVBool m_used;
} GVICaptureConverterData;

static GVIDeviceList GVIDevices;
static AudioStreamBasicDescription GVIVoiceFormat;

static void gviFreeCapturedFrames(GVICapturedFrame * frameHead)
{
	GVICapturedFrame * frame;
	while(frameHead->m_next)
	{
		frame = frameHead->m_next;
		frameHead->m_next = frame->m_next;
		gsifree(frame);
	}
}

static GVICapturedFrame * gviPopFirstFrame(GVICapturedFrame * frameHead)
{
	GVICapturedFrame * frame = frameHead->m_next;
	if(frame)
	{
		frameHead->m_next = frame->m_next;
		frame->m_next = NULL;
	}
	return frame;
}

static void gviPushFirstFrame(GVICapturedFrame * frameHead, GVICapturedFrame * frame)
{
	frame->m_next = frameHead->m_next;
	frameHead->m_next = frame;
}

static void gviPushLastFrame(GVICapturedFrame * frameHead, GVICapturedFrame * frame)
{
	GVICapturedFrame * lastFrame;

	lastFrame = frameHead;
	while(lastFrame->m_next)
		lastFrame = lastFrame->m_next;

	lastFrame->m_next = frame;
}

static GVBool gviLockDevice(GVIHardwareData * data)
{
	int rcode;
	rcode = pthread_mutex_lock(&data->m_mutex);
	return (rcode == 0)?GVTrue:GVFalse;
}

static void gviUnlockDevice(GVIHardwareData * data)
{
	pthread_mutex_unlock(&data->m_mutex);
}

static void gviCleanupPlayback(GVIHardwareData * data)
{
	if(data->m_playbackSources)
		gviFreeSourceList(data->m_playbackSources);
	if(data->m_playbackConverter)
		AudioConverterDispose(data->m_playbackConverter);
	gsifree(data->m_playbackBuffer);
}

static void gviCleanupCapture(GVIHardwareData * data)
{
	if(data->m_captureConverter)
		AudioConverterDispose(data->m_captureConverter);
	gviFreeCapturedFrames(&data->m_captureAvailableFrames);
	gviFreeCapturedFrames(&data->m_capturedFrames);
	gsifree(data->m_captureBuffer);
}

static void gviFreeArrayDevice(void * elem)
{
	GVIDevice * device = *(GVIDevice **)elem;
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;

	device->m_methods.m_stopDevice(device, device->m_types);

	pthread_mutex_destroy(&data->m_mutex);

	if(device->m_types & GV_PLAYBACK)
	{
		gviCleanupPlayback(data);
	}

	if(device->m_types & GV_CAPTURE)
	{
		gviCleanupCapture(data);
	}

	gviFreeDevice(device);
}

GVBool gviHardwareStartup(void)
{
	GVIDevices = gviNewDeviceList(gviFreeArrayDevice);
	if(!GVIDevices)
		return GVFalse;

	memset(&GVIVoiceFormat, 0, sizeof(AudioStreamBasicDescription));
	GVIVoiceFormat.mSampleRate = (Float64)GV_SAMPLES_PER_SECOND;
	GVIVoiceFormat.mFormatID = kAudioFormatLinearPCM;
	GVIVoiceFormat.mFormatFlags = kAudioFormatFlagsNativeEndian|kAudioFormatFlagIsSignedInteger|kAudioFormatFlagIsPacked;
	GVIVoiceFormat.mBytesPerPacket = GV_BYTES_PER_SAMPLE;
	GVIVoiceFormat.mFramesPerPacket = 1;
	GVIVoiceFormat.mBytesPerFrame = GV_BYTES_PER_SAMPLE;
	GVIVoiceFormat.mChannelsPerFrame = 1;
	GVIVoiceFormat.mBitsPerChannel = GV_BITS_PER_SAMPLE;

	return GVTrue;
}

void gviHardwareCleanup(void)
{
	if(GVIDevices)
	{
		gviFreeDeviceList(GVIDevices);
		GVIDevices = NULL;
	}
}

void gviHardwareThink(void)
{
	GVIDevice * device;
	int num;
	int i;

	if(!GVIDevices)
		return;

	num = gviGetNumDevices(GVIDevices);
	for(i = (num - 1) ; i >= 0 ; i--)
	{
		device = gviGetDevice(GVIDevices, i);

		if(((GVIHardwareData*)device->m_data)->m_unplugged)
		{
			gviDeviceUnplugged(device);
		}
	}
}

static GVBool gviCFStringToString(CFStringRef ref, gsi_char str[], int strLen)
{
#if !defined(GSI_UNICODE)

	Boolean result = CFStringGetCString(ref, str, strLen, kCFStringEncodingASCII);
	return (result == true)?GVTrue:GVFalse;

#else
	
	CFRange range;

	range.location = 0;
	range.length = CFStringGetLength(ref);
	range.length = min(range.length, strLen - 1);

	CFStringGetCharacters(ref, range, str);
	str[range.length] = '\0';

	return GVTrue;

#endif
}

static int gviCountChannels(AudioDeviceID deviceID, bool input)
{
	OSStatus result;
	UInt32 size;
	AudioBufferList * list;
	int numChannels;
	int i;

	result = AudioDeviceGetPropertyInfo(deviceID, 0, input, kAudioDevicePropertyStreamConfiguration, &size, NULL);
	if(result != noErr)
		return 0;

	list = (AudioBufferList *)gsimalloc(size);
	if(list == NULL)
		return 0;

	result = AudioDeviceGetProperty(deviceID, 0, input, kAudioDevicePropertyStreamConfiguration, &size, list);
	if(result != noErr)
	{
		gsifree(list);
		return 0;
	}

	numChannels = 0;
	for(i = 0 ; i < list->mNumberBuffers ; i++)
		numChannels += list->mBuffers[i].mNumberChannels;

	gsifree(list);

	return numChannels;
}

static GVBool gviFillDeviceInfo(AudioDeviceID deviceID, GVDeviceInfo * device, GVDeviceType types)
{
	OSStatus result;
	UInt32 size;
	UInt32 isAlive;
	GVDeviceType supportedTypes;
	CFStringRef nameRef;

	size = sizeof(UInt32);
	result = AudioDeviceGetProperty(deviceID, 0, true, kAudioDevicePropertyDeviceIsAlive, &size, &isAlive);
	if((result != noErr) || (isAlive != 1))
		return GVFalse;

	device->m_hardwareType = GVHardwareMacOSX;

	device->m_id = deviceID;

	supportedTypes = (GVDeviceType)0;
	if(gviCountChannels(deviceID, true) > 0)
		supportedTypes |= GV_CAPTURE;
	if(gviCountChannels(deviceID, false) > 0)
		supportedTypes |= GV_PLAYBACK;

	if(!(supportedTypes & types))
		return GVFalse;

	device->m_deviceType = supportedTypes;

	size = sizeof(nameRef);
	result = AudioDeviceGetProperty(deviceID, 0, (supportedTypes & GV_CAPTURE)?true:false, kAudioDevicePropertyDeviceNameCFString, &size, &nameRef);
	if(result != noErr)
		return GVFalse;

	if(CFStringGetLength(nameRef) <= 0)
	{
		CFRelease(nameRef);
		nameRef = NULL;
	}

	if(nameRef == NULL)
	{
		if(supportedTypes == GV_CAPTURE)
			nameRef = CFSTR("Capture Device");
		else if(supportedTypes == GV_PLAYBACK)
			nameRef = CFSTR("Playback Device");
		else
			nameRef = CFSTR("Capture & Playback Device");
	}

	gviCFStringToString(nameRef, device->m_name, GV_DEVICE_NAME_LEN);

	CFRelease(nameRef);

	return GVTrue;
}

int gviHardwareListDevices(GVDeviceInfo devices[], int maxDevices, GVDeviceType types)
{
	OSStatus result;
	UInt32 size;
	int numDevices;
	int deviceCount;
	AudioDeviceID * deviceIDs;
	AudioDeviceID defaultCaptureDeviceID;
	AudioDeviceID defaultPlaybackDeviceID;
	GVDeviceType defaultTypes;
	int i;

	size = sizeof(AudioDeviceID);
	result = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultInputDevice, &size, &defaultCaptureDeviceID);
	if(result != noErr)
		defaultCaptureDeviceID = kAudioDeviceUnknown;
	result = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &size, &defaultPlaybackDeviceID);
	if(result != noErr)
		defaultPlaybackDeviceID = kAudioDeviceUnknown;

	result = AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices, &size, NULL);
	if(result != noErr)
		return 0;

	numDevices = (size / sizeof(AudioDeviceID));

	deviceIDs = (AudioDeviceID *)gsimalloc(size);
	if(deviceIDs == NULL)
		return 0;

	result = AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &size, deviceIDs);
	if(result != noErr)
	{
		gsifree(deviceIDs);
		return 0;
	}

	deviceCount = 0;
	for(i = 0 ; (i < maxDevices) && (deviceCount < numDevices) ; i++)
	{
		GVBool addedDevice = gviFillDeviceInfo(deviceIDs[i], &devices[deviceCount], types);

		if(addedDevice)
		{
			defaultTypes = (GVDeviceType)0;
			if(deviceIDs[i] == defaultCaptureDeviceID)
				defaultTypes |= GV_CAPTURE;
			if(deviceIDs[i] == defaultPlaybackDeviceID)
				defaultTypes |= GV_PLAYBACK;
			devices[deviceCount].m_defaultDevice = defaultTypes;

			deviceCount++;
		}
	}

	gsifree(deviceIDs);

	return deviceCount;
}

static void gviHardwareFreeDevice(GVDevice device)
{
	gviDeleteDeviceFromList(GVIDevices, device);
}

static OSStatus gviAudioConverterCaptureProc(AudioConverterRef inAudioConverter,
                                             UInt32 * ioNumberDataPackets,
                                             AudioBufferList * ioData,
                                             AudioStreamPacketDescription ** outDataPacketDescription,
                                             void * inUserData)
{
	GVICaptureConverterData * converterData = (GVICaptureConverterData *)inUserData;
	GVIDevice * device = converterData->m_device;
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	AudioBufferList * bufferList = converterData->m_capturedAudio;

	if(converterData->m_used)
		return (OSStatus)1;

	ioData->mBuffers[0].mNumberChannels = bufferList->mBuffers[data->m_captureChannel].mNumberChannels;
	ioData->mBuffers[0].mData = bufferList->mBuffers[data->m_captureChannel].mData;
	ioData->mBuffers[0].mDataByteSize = bufferList->mBuffers[data->m_captureChannel].mDataByteSize;

	*ioNumberDataPackets = (ioData->mBuffers[0].mDataByteSize / data->m_captureStreamDescriptor.mBytesPerPacket);

	converterData->m_used = GVTrue;

	return noErr;
}

static OSStatus gviHardwareCaptureIOProc(AudioDeviceID inDevice,
                                         const AudioTimeStamp * inNow,
                                         const AudioBufferList * inInputData, 
                                         const AudioTimeStamp * inInputTime,
                                         AudioBufferList * outOutputData, 
                                         const AudioTimeStamp * inOutputTime,
                                         void * inClientData)
{
	GVIDevice * device = (GVIDevice *)inClientData;
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	OSStatus result;
	UInt32 size;
	GVICaptureConverterData converterData;
	AudioBufferList bufferList;
	GVICapturedFrame * frame;

	if(!gviLockDevice(data))
		return (OSStatus)1;

	if(data->m_capturing)
	{
		bufferList.mNumberBuffers = 1;
		bufferList.mBuffers[0].mNumberChannels = 1;
		bufferList.mBuffers[0].mDataByteSize = GVIBytesPerFrame;
		bufferList.mBuffers[0].mData = data->m_captureBuffer;

		converterData.m_device = device;
		converterData.m_capturedAudio = (AudioBufferList *)inInputData;
		converterData.m_used = GVFalse;

		do
		{
			size = GVISamplesPerFrame;

			result = AudioConverterFillComplexBuffer(data->m_captureConverter, gviAudioConverterCaptureProc, &converterData, &size, &bufferList, NULL);

			if(result == noErr)
			{
				frame = gviPopFirstFrame(&data->m_captureAvailableFrames);

				if(!frame)
				{
					frame = gviPopFirstFrame(&data->m_capturedFrames);
					assert(frame);
					if(!frame)
						break;
				}

				memcpy(frame->m_frame, data->m_captureBuffer, GVIBytesPerFrame);
				frame->m_frameStamp = data->m_captureClock;

				data->m_captureClock++;

				gviPushLastFrame(&data->m_capturedFrames, frame);
			}
		}
		while(result == noErr);
	}

	gviUnlockDevice(data);

	return noErr;
}

static GVBool gviHardwareStartCapture(GVDevice device)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	OSStatus result;

	data->m_capturing = GVTrue;

	result = AudioDeviceAddIOProc((AudioDeviceID)device->m_deviceID, gviHardwareCaptureIOProc, device);
	if(result != noErr)
	{
		data->m_capturing = GVFalse;
		return GVFalse;
	}

	result = AudioDeviceStart((AudioDeviceID)device->m_deviceID, gviHardwareCaptureIOProc);
	if(result != noErr)
	{
		data->m_capturing = GVFalse;
		return GVFalse;
	}

	return GVTrue;
}

static OSStatus gviAudioConverterPlaybackProc(AudioConverterRef inAudioConverter,
                                              UInt32 * ioNumberDataPackets,
                                              AudioBufferList * ioData,
                                              AudioStreamPacketDescription ** outDataPacketDescription,
                                              void * inUserData)
{
	GVIDevice * device = (GVIDevice *)inUserData;
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	GVBool wroteToBuffer = GVFalse;

	if(data->m_playing)
	{
		wroteToBuffer = gviWriteSourcesToBuffer(data->m_playbackSources, data->m_playbackClock, data->m_playbackBuffer, 1);
	}

	if(!data->m_playing || !wroteToBuffer)
	{
		memset(data->m_playbackBuffer, 0, (size_t)GVIBytesPerFrame);
	}
	else if(data->m_playbackVolume < 1.0)
	{
		int i;
		for(i = 0 ; i < GVISamplesPerFrame ; i++)
			data->m_playbackBuffer[i] = (GVSample)(data->m_playbackBuffer[i] * data->m_playbackVolume);
	}

	if(device->m_playbackFilterCallback)
		device->m_playbackFilterCallback(device, data->m_playbackBuffer, data->m_playbackClock);

	ioData->mBuffers[0].mNumberChannels = 1;
	ioData->mBuffers[0].mDataByteSize = GVIBytesPerFrame;
	ioData->mBuffers[0].mData = (void *)data->m_playbackBuffer;

	*ioNumberDataPackets = GVISamplesPerFrame;

	data->m_playbackClock++;

	return noErr;
}

static OSStatus gviHardwarePlaybackIOProc(AudioDeviceID inDevice,
                                          const AudioTimeStamp * inNow,
                                          const AudioBufferList * inInputData, 
                                          const AudioTimeStamp * inInputTime,
                                          AudioBufferList * outOutputData, 
                                          const AudioTimeStamp * inOutputTime,
                                          void * inClientData)
{
	GVIDevice * device = (GVIDevice *)inClientData;
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	AudioBufferList bufferList;
	OSStatus result;
	UInt32 size;

	if(!gviLockDevice(data))
		return (OSStatus)1;

	size = (outOutputData->mBuffers[0].mDataByteSize / data->m_playbackStreamDescriptor.mBytesPerFrame);

	bufferList.mNumberBuffers = 1;
	bufferList.mBuffers[0] = outOutputData->mBuffers[data->m_playbackChannel];

	result = AudioConverterFillComplexBuffer(data->m_playbackConverter, gviAudioConverterPlaybackProc, device, &size, &bufferList, NULL);
	if(result != noErr)
		return (OSStatus)1;

	gviUnlockDevice(data);

	return noErr;
}

static GVBool gviHardwareStartPlayback(GVDevice device)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	OSStatus result;

	data->m_playing = GVTrue;

	data->m_playbackClock = 0;

	result = AudioDeviceAddIOProc((AudioDeviceID)device->m_deviceID, gviHardwarePlaybackIOProc, device);
	if(result != noErr)
	{
		data->m_playing = GVFalse;
		return GVFalse;
	}

	result = AudioDeviceStart((AudioDeviceID)device->m_deviceID, gviHardwarePlaybackIOProc);
	if(result != noErr)
	{
		data->m_playing = GVFalse;
		return GVFalse;
	}

	return GVTrue;
}

static GVBool gviHardwareStartDevice(GVDevice device, GVDeviceType type)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	GVBool result;

	if(!gviLockDevice(data))
		return GVFalse;

	if((type & GV_CAPTURE) && !data->m_capturing)
	{
		result = gviHardwareStartCapture(device);
		if(!result)
		{
			gviUnlockDevice(data);
			return GVFalse;
		}
	}

	if((type & GV_PLAYBACK) && !data->m_playing)
	{
		result = gviHardwareStartPlayback(device);
		if(!result)
		{
			if(type & GV_CAPTURE)
				device->m_methods.m_stopDevice(device, GV_CAPTURE);
			gviUnlockDevice(data);
			return GVFalse;
		}
	}

	gviUnlockDevice(data);

	return GVTrue;
}

static void gviHardwareStopDevice(GVDevice device, GVDeviceType type)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	GVICapturedFrame * frame;

	gviLockDevice(data);

	if((type & GV_CAPTURE) && data->m_capturing)
	{
		gviUnlockDevice(data);

		AudioDeviceStop((AudioDeviceID)device->m_deviceID, gviHardwareCaptureIOProc);

		gviLockDevice(data);

		AudioDeviceRemoveIOProc((AudioDeviceID)device->m_deviceID, gviHardwareCaptureIOProc);

		while((frame = gviPopFirstFrame(&data->m_capturedFrames)) != NULL)
			gviPushFirstFrame(&data->m_captureAvailableFrames, frame);

		data->m_capturing = GVFalse;
	}

	if((type & GV_PLAYBACK) && data->m_playing)
	{
		gviUnlockDevice(data);

		AudioDeviceStop((AudioDeviceID)device->m_deviceID, gviHardwarePlaybackIOProc);

		gviLockDevice(data);

		AudioDeviceRemoveIOProc((AudioDeviceID)device->m_deviceID, gviHardwarePlaybackIOProc);

		gviClearSourceList(data->m_playbackSources);

		data->m_playing = GVFalse;
	}

	gviUnlockDevice(data);
}

static GVBool gviHardwareIsDeviceStarted(GVDevice device, GVDeviceType type)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	GVBool result = GVFalse;

	if(!gviLockDevice(data))
		return GVFalse;

	if(type == GV_PLAYBACK)
		result = data->m_playing;
	else if(type == GV_CAPTURE)
		result = data->m_capturing;
	else if (type == GV_CAPTURE_AND_PLAYBACK)
		result = (data->m_playing && data->m_capturing)?GVTrue:GVFalse;

	gviUnlockDevice(data);

	return result;
}

#if GVI_VOLUME_IN_SOFTWARE

	static void gviSetDeviceVolume(GVDevice device, GVBool isInput, GVScalar volume)
	{
		GVIHardwareData * data = (GVIHardwareData *)device->m_data;

		if(isInput)
			data->m_captureVolume = volume;
		else
			data->m_playbackVolume = volume;
	}

	static GVScalar gviGetDeviceVolume(GVDevice device, GVBool isInput)
	{
		GVIHardwareData * data = (GVIHardwareData *)device->m_data;

		if(isInput)
			return data->m_captureVolume;
		else
			return data->m_playbackVolume;
	}

#else

	static void gviSetDeviceVolume(GVDevice device, GVBool isInput, GVScalar volume)
	{
		Float32 vol = volume;
		int channel;

		for(channel = 0 ; channel < 3 ; channel++)
			AudioDeviceSetProperty(device->m_deviceID, NULL, channel, isInput, kAudioDevicePropertyVolumeScalar, sizeof(Float32), &vol);
	}

	static GVBool gviGetChannelVolume(GVDevice device, GVBool isInput, int channel, Float32 * volume)
	{
		OSStatus result;
		UInt32 size = sizeof(Float32);

		result = AudioDeviceGetProperty(device->m_deviceID, channel, isInput, kAudioDevicePropertyVolumeScalar, &size, volume);

		return (result == noErr)?GVTrue:GVFalse;
	}

	static GVScalar gviGetDeviceVolume(GVDevice device, GVBool isInput)
	{
		GVBool result;
		Float32 channels[2];

		result = gviGetChannelVolume(device, isInput, 0, &channels[0]);
		if(result)
			return (GVScalar)channels[0];

		result = gviGetChannelVolume(device, isInput, 1, &channels[0]);
		if(!result)
			return 0;

		result = gviGetChannelVolume(device, isInput, 2, &channels[1]);
		if(!result)
			return (GVScalar)channels[0];

		return (GVScalar)((channels[0] + channels[1]) / 2);
	}

#endif

static void gviHardwareSetDeviceVolume(GVDevice device, GVDeviceType type, GVScalar volume)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;

	if(!gviLockDevice(data))
		return;

	if(type & GV_PLAYBACK)
		gviSetDeviceVolume(device, GVFalse, volume);
	if(type & GV_CAPTURE)
		gviSetDeviceVolume(device, GVTrue, volume);

	gviUnlockDevice(data);
}

static GVScalar gviHardwareGetDeviceVolume(GVDevice device, GVDeviceType type)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	GVScalar volume = 0;

	if(!gviLockDevice(data))
		return 0;

	if(type & GV_PLAYBACK)
		volume = gviGetDeviceVolume(device, GVFalse);
	else if(type & GV_CAPTURE)
		volume = gviGetDeviceVolume(device, GVTrue);

	gviUnlockDevice(data);

	return volume;
}

static void gviHardwareSetCaptureThreshold(GVDevice device, GVScalar threshold)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;

	if(!gviLockDevice(data))
		return;

	data->m_captureThreshold = threshold;

	gviUnlockDevice(data);
}

static GVScalar gviHardwareGetCaptureThreshold(GVDevice device)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	GVScalar threshold;

	if(!gviLockDevice(data))
		return GVFalse;

	threshold = data->m_captureThreshold;
	
	gviUnlockDevice(data);
	
	return threshold;
}

static GVBool gviHardwareCapturePacket(GVDevice device, GVByte * packet, int * len, GVFrameStamp * frameStamp, GVScalar * volume)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	int numFrames;
	GVICapturedFrame * frame;
	GVScalar frameVolume;
	GVBool overThreshold;

	if(!gviLockDevice(data))
		return GVFalse;

	numFrames = (*len / GVIEncodedFrameSize);

	*len = 0;
	if(volume)
		*volume = 0;

	if(!data->m_capturing)
	{
		gviUnlockDevice(data);
		return GVFalse;
	}

	if(numFrames < 1)
	{
		gviUnlockDevice(data);
		return GVFalse;
	}

	do
	{
		frame = gviPopFirstFrame(&data->m_capturedFrames);
		if(!frame)
		{
			gviUnlockDevice(data);
			return GVFalse;
		}

		frameVolume = gviGetSamplesVolume(frame->m_frame, GVISamplesPerFrame);

		overThreshold = (frameVolume >= data->m_captureThreshold);
		if(overThreshold)
		{
			data->m_captureLastCrossedThresholdTime = frame->m_frameStamp;
		}
		else
		{
			overThreshold = ((GVFrameStamp)(frame->m_frameStamp - data->m_captureLastCrossedThresholdTime) < GVI_HOLD_THRESHOLD_FRAMES);
		}

		if(overThreshold)
		{
			if(data->m_captureVolume < 1.0)
			{
				int j;
				for(j = 0 ; j < GVISamplesPerFrame ; j++)
					frame->m_frame[j] = (GVSample)(frame->m_frame[j] * data->m_captureVolume);
			}

			if(device->m_captureFilterCallback)
				device->m_captureFilterCallback(device, frame->m_frame, frame->m_frameStamp);

			gviEncode(packet, frame->m_frame);
			*len = GVIEncodedFrameSize;
			*frameStamp = frame->m_frameStamp;
			if(volume)
				*volume = frameVolume;
		}

		gviPushFirstFrame(&data->m_captureAvailableFrames, frame);
	}
	while(!overThreshold);
	
	gviUnlockDevice(data);

	return GVTrue;
}

static int gviHardwareGetAvailableCaptureBytes(GVDevice device)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	GVICapturedFrame * frame;
	int count = 0;

	if(!data->m_capturing)
		return 0;

	for(frame = data->m_capturedFrames.m_next ; frame ; frame = frame->m_next)
		count++;

	return (count * GVIEncodedFrameSize);
}

static void gviHardwarePlayPacket(GVDevice device, const GVByte * packet, int len, GVSource source, GVFrameStamp frameStamp, GVBool mute)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;

	if(!gviLockDevice(data))
		return;

	if(data->m_playing)
	{
		gviAddPacketToSourceList(data->m_playbackSources, packet, len, source, frameStamp, mute, data->m_playbackClock);
	}

	gviUnlockDevice(data);
}

static GVBool gviHardwareIsSourceTalking(GVDevice device, GVSource source)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	GVBool isTalking = GVFalse;
	
	if(!gviLockDevice(data))
		return GVFalse;

	if(data->m_playing)
	{
		isTalking = gviIsSourceTalking(data->m_playbackSources, source);
	}
	
	gviUnlockDevice(data);
	
	return isTalking;
}

static int gviHardwareListTalkingSources(GVDevice device, GVSource sources[], int maxSources)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	int numTalking = 0;

	if(!gviLockDevice(data))
		return 0;

	if(data->m_playing)
	{
		numTalking = gviListTalkingSources(data->m_playbackSources, sources, maxSources);
	}
	
	gviUnlockDevice(data);
	
	return numTalking;
}

static int gviHardwareGetNumChannels(GVDevice device, GVDeviceType type)
{
	OSStatus result;
	UInt32 size;
	int numStreams;

	result = AudioDeviceGetPropertyInfo(device->m_deviceID, 0, (type & GV_CAPTURE)?true:false, kAudioDevicePropertyStreams, &size, NULL);
	if(result != noErr)
		return 1;

	numStreams = (size / sizeof(AudioStreamID));

	return numStreams;
}

static void gviHardwareGetChannelName(GVDevice device, GVDeviceType type, int index, gsi_char name[GV_CHANNEL_NAME_LEN])
{
	OSStatus result;
	UInt32 size;
	CFStringRef nameRef;
	AudioStreamID * streamIDs;
	int num;

	name[0] = '\0';

	num = gviHardwareGetNumChannels(device, type);

	if((index < 0) || (index >= num))
		return;

	size = (num * sizeof(AudioStreamID));
	streamIDs = (AudioStreamID*)gsimalloc(size);

	result = AudioDeviceGetProperty(device->m_deviceID, 0, (type & GV_CAPTURE)?true:false, kAudioDevicePropertyStreams, &size, streamIDs);
	if(result == noErr)
	{
		size = sizeof(CFStringRef);
		result = AudioStreamGetProperty(streamIDs[index], 0, kAudioDevicePropertyDeviceNameCFString, &size, &nameRef);
		if(result != noErr)
			nameRef = NULL;
	}

	if(nameRef && (CFStringGetLength(nameRef) <= 0))
	{
		CFRelease(nameRef);
		nameRef = NULL;
	}

	if(nameRef == NULL)
	{
		CFStringRef format;

		if(type & GV_CAPTURE)
			format = CFSTR("Input Channel %d");
		else
			format = CFSTR("Output Channel %d");

		nameRef = CFStringCreateWithFormat(NULL, NULL, format, index);

		CFRelease(format);
	}

	gviCFStringToString(nameRef, name, GV_CHANNEL_NAME_LEN);

	CFRelease(nameRef);

	gsifree(streamIDs);
}

static void gviHardwareSetChannel(GVDevice device, GVDeviceType type, int index)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	int num;
	
	num = gviHardwareGetNumChannels(device, type);

	if((index < 0) || (index >= num))
		return;

	if(type & GV_CAPTURE)
		data->m_captureChannel = index;
	else
		data->m_playbackChannel = index;
}

static int gviHardwareGetChannel(GVDevice device, GVDeviceType type)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;

	if(type & GV_CAPTURE)
		return data->m_captureChannel;
	else
		return data->m_playbackChannel;
}

OSStatus gviPropertyListener(AudioDeviceID           inDevice,
                             UInt32                  inChannel,
                             Boolean                 isInput,
                             AudioDevicePropertyID   inPropertyID,
                             void*                   inClientData)
{
	GVIDevice * device = (GVIDevice*)inClientData;
	GVIHardwareData * data = (GVIHardwareData*)device->m_data;
	UInt32 value;
	UInt32 len = sizeof(UInt32);
	OSStatus result;

	result = AudioDeviceGetProperty(device->m_deviceID, 0, isInput, kAudioDevicePropertyDeviceIsAlive, &len, &value);
	if((result == noErr) && (value == 0))
	{
		gviLockDevice(device->m_data);
		data->m_unplugged = GVTrue;
		gviUnlockDevice(device->m_data);
	}

	return (OSStatus)0;
}

static GVBool gviHardwareInitCapture(GVIDevice * device)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	UInt32 size;
	OSStatus result;
	GVICapturedFrame * frame;
	int numCaptureBufferBytes;
	int numCaptureBufferFrames;
	int i;

	size = sizeof(AudioStreamBasicDescription);
	result = AudioDeviceGetProperty(device->m_deviceID, 0, true, kAudioDevicePropertyStreamFormat, &size, &data->m_captureStreamDescriptor);
	if(result != noErr)
		return GVFalse;

	result = AudioConverterNew(&data->m_captureStreamDescriptor, &GVIVoiceFormat, &data->m_captureConverter);
	if(result != noErr)
		return GVFalse;

	data->m_captureBuffer = (GVSample *)gsimalloc(GVIBytesPerFrame);
	if(!data->m_captureBuffer)
	{
		AudioConverterDispose(data->m_captureConverter);
		return GVFalse;		
	}

	numCaptureBufferBytes = gviMultiplyByBytesPerMillisecond(GVI_CAPTURE_BUFFER_MILLISECONDS);
	numCaptureBufferBytes = gviRoundUpToNearestMultiple(numCaptureBufferBytes, GVIBytesPerFrame);
	numCaptureBufferFrames = (numCaptureBufferBytes / GVIBytesPerFrame);
	for(i = 0 ; i < numCaptureBufferFrames ; i++)
	{
		frame = (GVICapturedFrame *)gsimalloc(sizeof(GVICapturedFrame) + GVIBytesPerFrame - sizeof(GVSample));
		if(!frame)
		{
			gviFreeCapturedFrames(&data->m_captureAvailableFrames);
			gsifree(data->m_captureBuffer);
			AudioConverterDispose(data->m_captureConverter);
			return GVFalse;
		}
		gviPushFirstFrame(&data->m_captureAvailableFrames, frame);
	}

	data->m_captureLastCrossedThresholdTime = (data->m_captureClock - GVI_HOLD_THRESHOLD_FRAMES - 1);

	AudioDeviceAddPropertyListener(device->m_deviceID, 0, true, kAudioDevicePropertyDeviceIsAlive, gviPropertyListener, device);

#if GVI_VOLUME_IN_SOFTWARE
	data->m_captureVolume = (GVScalar)1.0;
#endif

	return GVTrue;
}

static GVBool gviHardwareInitPlayback(GVIDevice * device)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	UInt32 size;
	OSStatus result;
	UInt32 primeMethod;
	SInt32 channelMap[100];
	int i;

	data->m_playbackSources = gviNewSourceList();
	if(!data->m_playbackSources)
		return GVFalse;

	size = sizeof(AudioStreamBasicDescription);
	result = AudioDeviceGetProperty(device->m_deviceID, 0, false, kAudioDevicePropertyStreamFormat, &size, &data->m_playbackStreamDescriptor);
	if(result != noErr)
	{
		gviFreeSourceList(data->m_playbackSources);
		return GVFalse;
	}

	result = AudioConverterNew(&GVIVoiceFormat, &data->m_playbackStreamDescriptor, &data->m_playbackConverter);
	if(result != noErr)
	{
		gviFreeSourceList(data->m_playbackSources);
		return GVFalse;
	}

	primeMethod = kConverterPrimeMethod_None;
	result = AudioConverterSetProperty(data->m_playbackConverter, kAudioConverterPrimeMethod, sizeof(UInt32), &primeMethod);
	if(result != noErr)
	{
		AudioConverterDispose(data->m_playbackConverter);
		gviFreeSourceList(data->m_playbackSources);
		return GVFalse;
	}

	result = AudioConverterGetPropertyInfo(data->m_playbackConverter, kAudioConverterChannelMap, &size, NULL);
	if(result == noErr)
	{
		result = AudioConverterGetProperty(data->m_playbackConverter, kAudioConverterChannelMap, &size, channelMap);
		if(result == noErr)
		{
			for(i = 0 ; i < (size / sizeof(SInt32)) ; i++)
				channelMap[i] = 0;

			AudioConverterSetProperty(data->m_playbackConverter, kAudioConverterChannelMap, size, channelMap);
		}
	}

	data->m_playbackBuffer = (GVSample *)gsimalloc(GVIBytesPerFrame);
	if(!data->m_playbackBuffer)
	{
		AudioConverterDispose(data->m_playbackConverter);
		gviFreeSourceList(data->m_playbackSources);
		return GVFalse;
	}

	AudioDeviceAddPropertyListener(device->m_deviceID, 0, false, kAudioDevicePropertyDeviceIsAlive, gviPropertyListener, device);

#if GVI_VOLUME_IN_SOFTWARE
	data->m_playbackVolume = (GVScalar)1.0;
#endif

	return GVTrue;
}

static GVBool gviHardwareInitDevice(GVIDevice * device)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	int result;

	result = pthread_mutex_init(&data->m_mutex, NULL);
	if(result != 0)
		return GVFalse;

	if(device->m_types & GV_PLAYBACK)
	{
		if(!gviHardwareInitPlayback(device))
		{
			pthread_mutex_destroy(&data->m_mutex);
			return GVFalse;
		}
	}

	if(device->m_types & GV_CAPTURE)
	{
		if(!gviHardwareInitCapture(device))
		{
			pthread_mutex_destroy(&data->m_mutex);
			gviCleanupPlayback(data);
			return GVFalse;
		}
	}

	return GVTrue;
}

GVDevice gviHardwareNewDevice(GVDeviceID deviceID, GVDeviceType type)
{
	GVIDevice * device;
	GVBool result;

	if(!(type & GV_CAPTURE_AND_PLAYBACK))
		return NULL;

	device = gviNewDevice(deviceID, GVHardwareMacOSX, type, sizeof(GVIHardwareData));
	if(!device)
		return NULL;

	result = gviHardwareInitDevice(device);
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
	device->m_methods.m_playPacket = gviHardwarePlayPacket;
	device->m_methods.m_isSourceTalking = gviHardwareIsSourceTalking;
	device->m_methods.m_listTalkingSources = gviHardwareListTalkingSources;
	device->m_methods.m_getNumChannels = gviHardwareGetNumChannels;
	device->m_methods.m_getChannelName = gviHardwareGetChannelName;
	device->m_methods.m_setChannel= gviHardwareSetChannel;
	device->m_methods.m_getChannel = gviHardwareGetChannel;

	gviAppendDeviceToList(GVIDevices, device);

	return device;
}
