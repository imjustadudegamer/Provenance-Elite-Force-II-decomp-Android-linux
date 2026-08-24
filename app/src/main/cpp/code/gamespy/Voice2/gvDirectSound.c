/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#include <stdio.h>
#include <math.h>
#include "gvDirectSound.h"
#include "gvDevice.h"
#include "gvCodec.h"
#include "gvSource.h"
#include "gvUtil.h"
#pragma warning(disable:4201)
#include <mmsystem.h>
#include <dsound.h>
#if (DIRECTSOUND_VERSION == 0x0800)
#include <dvoice.h>
#endif

#if !defined(_WIN32)
#error This file should only be used with Windows
#endif

#define GVI_GUID_COPY(dest, src)          memcpy((dest), (src), sizeof(GUID))

const GUID GVDefaultPlaybackDeviceID = {0xdef00002, 0x9c6d, 0x47ed, 0xaa, 0xf1, 0x4d, 0xda, 0x8f, 0x2b, 0x5c, 0x03};
const GUID GVDefaultCaptureDeviceID  = {0xdef00003, 0x9c6d, 0x47ed, 0xaa, 0xf1, 0x4d, 0xda, 0x8f, 0x2b, 0x5c, 0x03};

typedef struct
{
	GVBool m_playing;
	LPDIRECTSOUND m_playback;
	LPDIRECTSOUNDBUFFER m_playbackBuffer;
	DWORD m_playbackBufferSize;
	DWORD m_playbackBufferHalfSize;
	DWORD m_playbackBufferPosition;
	GVFrameStamp m_playbackClock;
	GVISourceList m_playbackSources;
	gsi_time m_playbackLastThink;

	GVBool m_capturing;
	LPDIRECTSOUNDCAPTURE m_capture;
	LPDIRECTSOUNDCAPTUREBUFFER m_captureBuffer;
	DWORD m_captureBufferSize;
	DWORD m_captureBufferPosition;
	GVScalar m_captureVolume;
	GVFrameStamp m_captureClock;
	GVScalar m_captureThreshold;
	GVFrameStamp m_captureLastCrossedThresholdTime;
} GVIHardwareData;

typedef struct
{
	GVDeviceInfo * m_devices;
	int m_len;
	int m_num;
	GVBool m_enumeratingCapture;
} GVIEnumDevicesInfo;

static HWND GVIHwnd;
static GVIDeviceList GVIDevices;
static WAVEFORMATEX GVIWaveFormat;
static GVBool GVICleanupCOM;

static void gviFreeArrayDevice(void * elem)
{
	GVIDevice * device = *(GVIDevice **)elem;
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;

	if(device->m_types == GV_PLAYBACK)
	{
		if(data->m_playbackSources)
			gviFreeSourceList(data->m_playbackSources);
		if(data->m_playbackBuffer)
			IDirectSoundBuffer_Release(data->m_playbackBuffer);
		if(data->m_playback)
			IDirectSound_Release(data->m_playback);
	}
	else
	{
		if(data->m_captureBuffer)
		{
			IDirectSoundCaptureBuffer_Stop(data->m_captureBuffer);
			IDirectSoundCaptureBuffer_Release(data->m_captureBuffer);
		}
		if(data->m_capture)
			IDirectSoundCapture_Release(data->m_capture);
	}

	gviFreeDevice(device);
}

GVBool gviHardwareStartup(HWND hWnd)
{
	HRESULT result;

	if(!hWnd)
	{
		hWnd = GetForegroundWindow();
		if(!hWnd)
			hWnd = GetDesktopWindow();
	}

	GVIHwnd = hWnd;

	GVIDevices = gviNewDeviceList(gviFreeArrayDevice);
	if(!GVIDevices)
		return GVFalse;

	assert(GVICleanupCOM == GVFalse);
	result = CoInitialize(NULL);
	if(SUCCEEDED(result))
		GVICleanupCOM = GVTrue;

	return GVTrue;
}

void gviHardwareCleanup(void)
{
	if(GVIDevices)
	{
		gviFreeDeviceList(GVIDevices);
		GVIDevices = NULL;
	}

	if(GVICleanupCOM == GVTrue)
	{
		CoUninitialize();
		GVICleanupCOM = GVFalse;
	}
}

static GVBool gviPlaybackDeviceThink(GVIDevice * device)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	HRESULT result;
	DWORD lockPosition;
	LPVOID audioPtr1, audioPtr2;
	DWORD audioLen1, audioLen2;
	GVBool wroteToBuffer;
	DWORD newBytes;
	DWORD rawPlayCursor;
	DWORD playCursor;
	DWORD writeCursor;
	gsi_time now;
	int diff;
	int numFrames;
	int i;

	if(!data->m_playing)
		return GVTrue;

	result = IDirectSoundBuffer_GetCurrentPosition(data->m_playbackBuffer, &rawPlayCursor, &writeCursor);
	if(FAILED(result))
		return GVFalse;

	playCursor = (DWORD)gviRoundDownToNearestMultiple((int)rawPlayCursor, GVIBytesPerFrame);

	newBytes = (((playCursor + data->m_playbackBufferSize) - data->m_playbackBufferPosition) % data->m_playbackBufferSize);

	lockPosition = data->m_playbackBufferPosition;

	data->m_playbackBufferPosition = playCursor;

	now = current_time();
	diff = (int)(now - data->m_playbackLastThink);
	data->m_playbackLastThink = now;

	diff -= gviDivideByBytesPerMillisecond(newBytes);

	diff -= (rawPlayCursor - playCursor);

	if(diff >= gviDivideByBytesPerMillisecond((int)data->m_playbackBufferHalfSize))
	{
		int numMissedLoops;
		int numMissedFrames;
		int msPerBuffer = gviDivideByBytesPerMillisecond((int)data->m_playbackBufferSize);

		numMissedLoops = (gviRoundToNearestMultiple(diff, msPerBuffer) / msPerBuffer);

		numMissedFrames = (numMissedLoops * (data->m_playbackBufferSize / GVIBytesPerFrame));

		data->m_playbackClock = data->m_playbackClock + (GVFrameStamp)(numMissedFrames);
	}

	if(newBytes == 0)
		return GVTrue;

	result = IDirectSoundBuffer_Lock(data->m_playbackBuffer, lockPosition, newBytes, &audioPtr1, &audioLen1, &audioPtr2, &audioLen2, 0);
	if(FAILED(result))
		return GVFalse;

	numFrames = (audioLen1 / GVIBytesPerFrame);
	wroteToBuffer = gviWriteSourcesToBuffer(data->m_playbackSources, data->m_playbackClock, (GVSample *)audioPtr1, numFrames);
	if(!wroteToBuffer)
		memset(audioPtr1, 0, audioLen1);

	if(device->m_playbackFilterCallback)
	{
		for(i = 0 ; i < numFrames ; i++)
			device->m_playbackFilterCallback(device, (GVSample *)audioPtr1 + (GVISamplesPerFrame * i), (GVFrameStamp)(data->m_playbackClock + i));
	}

	data->m_playbackClock = data->m_playbackClock + (GVFrameStamp)numFrames;

	if(audioPtr2)
	{
		numFrames = (audioLen2 / GVIBytesPerFrame);
		wroteToBuffer = gviWriteSourcesToBuffer(data->m_playbackSources, data->m_playbackClock, (GVSample *)audioPtr2, numFrames);
		if(!wroteToBuffer)
			memset(audioPtr2, 0, audioLen2);

		if(device->m_playbackFilterCallback)
		{
			for(i = 0 ; i < numFrames ; i++)
				device->m_playbackFilterCallback(device, (GVSample *)audioPtr2 + (GVISamplesPerFrame * i), (GVFrameStamp)(data->m_playbackClock + i));
		}

		data->m_playbackClock = data->m_playbackClock + (GVFrameStamp)numFrames;
	}

	result = IDirectSoundBuffer_Unlock(data->m_playbackBuffer, audioPtr1, audioLen1, audioPtr2, audioLen2);
	if(FAILED(result))
		return GVFalse;

	return GVTrue;
}

void gviHardwareThink(void)
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

		if(device->m_types == GV_PLAYBACK)
		{
			rcode = gviPlaybackDeviceThink(device);

			if(!rcode)
				gviDeviceUnplugged(device);
		}
	}
}

#if !defined(GSI_UNICODE)
	static BOOL CALLBACK gviEnumDevicesCallback(LPGUID lpGuid, LPCSTR desc, LPCSTR module, LPVOID lpContext)
#else
	static BOOL CALLBACK gviEnumDevicesCallback(LPGUID lpGuid, LPCWSTR desc, LPCWSTR module, LPVOID lpContext)
#endif
{
	if(lpGuid != NULL)
	{
		GVIEnumDevicesInfo * info = (GVIEnumDevicesInfo *)lpContext;
		GVDeviceInfo * device = NULL;
		int i;

		for(i = 0 ; i < info->m_num ; i++)
		{
			if(IsEqualGUID(&info->m_devices[i].m_id, lpGuid))
				device = &info->m_devices[i];
		}

		if(!device)
		{
			device = &info->m_devices[info->m_num++];

			memset(device, 0, sizeof(GVDeviceInfo));

			GVI_GUID_COPY(&device->m_id, lpGuid);
			_tcsncpy(device->m_name, desc, GV_DEVICE_NAME_LEN);
			device->m_name[GV_DEVICE_NAME_LEN - 1] = '\0';
		}

		if(info->m_enumeratingCapture)
			device->m_deviceType = GV_CAPTURE;
		else
			device->m_deviceType = GV_PLAYBACK;

		device->m_hardwareType = GVHardwareDirectSound;

		if(info->m_num == info->m_len)
			return FALSE;
	}
	GSI_UNUSED(module);
	return TRUE;
}

int gviHardwareListDevices(GVDeviceInfo devices[], int maxDevices, GVDeviceType types)
{
#if DIRECTSOUND_VERSION >= 0x0800
	GUID defaultCaptureDevice;
	GUID defaultPlaybackDevice;
#endif
	GVIEnumDevicesInfo info;
	int i;

	if(maxDevices < 1)
		return 0;

	info.m_devices = devices;
	info.m_len = maxDevices;
	info.m_num = 0;
	if(types & GV_CAPTURE)
	{
		info.m_enumeratingCapture = GVTrue;
#if !defined(GSI_UNICODE)
		DirectSoundCaptureEnumerateA(gviEnumDevicesCallback, &info);
#else
		DirectSoundCaptureEnumerateW(gviEnumDevicesCallback, &info);
#endif
	}
	if(types & GV_PLAYBACK)
	{
		info.m_enumeratingCapture = GVFalse;
#if !defined(GSI_UNICODE)
		DirectSoundEnumerateA(gviEnumDevicesCallback, &info);
#else
		DirectSoundEnumerateW(gviEnumDevicesCallback, &info);
#endif
	}

#if DIRECTSOUND_VERSION >= 0x0800
	GetDeviceID(&DSDEVID_DefaultVoiceCapture, &defaultCaptureDevice);
	GetDeviceID(&DSDEVID_DefaultVoicePlayback, &defaultPlaybackDevice);
	for(i = 0 ; i < info.m_num ; i++)
	{
		if(IsEqualGUID(&devices[i].m_id, &defaultCaptureDevice))
			devices[i].m_defaultDevice = GV_CAPTURE;
		else if(IsEqualGUID(&devices[i].m_id, &defaultPlaybackDevice))
			devices[i].m_defaultDevice = GV_PLAYBACK;
	}
#else
	for(i = 0 ; i < info.m_num ; i++)
	{
		devices[i].m_defaultDevice = (GVDeviceType)0;
	}
#endif

	return info.m_num;
}

static void gviHardwareFreeDevice(GVIDevice * device)
{
	gviDeleteDeviceFromList(GVIDevices, device);
}

static GVBool gviStartPlaybackDevice(GVIHardwareData * data)
{
	HRESULT result;
	LPVOID audioPtr1, audioPtr2;
	DWORD audioLen1, audioLen2;

	IDirectSoundBuffer_Stop(data->m_playbackBuffer);

	result = IDirectSoundBuffer_Lock(data->m_playbackBuffer, 0, 0, &audioPtr1, &audioLen1, &audioPtr2, &audioLen2, DSBLOCK_ENTIREBUFFER);
	if(FAILED(result))
		return GVFalse;

	memset(audioPtr1, 0, audioLen1);

	result = IDirectSoundBuffer_Unlock(data->m_playbackBuffer, audioPtr1, audioLen1, audioPtr2, audioLen2);
	if(FAILED(result))
		return GVFalse;

	result = IDirectSoundBuffer_SetCurrentPosition(data->m_playbackBuffer, 0);
	if(FAILED(result))
		return GVFalse;

	result = IDirectSoundBuffer_Play(data->m_playbackBuffer, 0, 0, DSBPLAY_LOOPING);
	if(FAILED(result))
		return GVFalse;

	data->m_playbackClock = 0;

	data->m_playbackLastThink = current_time();

	data->m_playing = GVTrue;

	return GVTrue;
}

static GVBool gviStartCaptureDevice(GVIHardwareData * data)
{
	data->m_capturing = GVTrue;

	return GVTrue;
}

static GVBool gviHardwareStartDevice(GVIDevice * device, GVDeviceType type)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;

	if(type == GV_PLAYBACK)
		return gviStartPlaybackDevice(data);
	return gviStartCaptureDevice(data);
}

static void gviStopPlaybackDevice(GVIHardwareData * data)
{
	IDirectSoundBuffer_Stop(data->m_playbackBuffer);

	data->m_playing = GVFalse;

	gviClearSourceList(data->m_playbackSources);
}

static void gviStopCaptureDevice(GVIHardwareData * data)
{
	data->m_capturing = GVFalse;

	gviResetEncoder();
}

static void gviHardwareStopDevice(GVIDevice * device, GVDeviceType type)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;

	if(type == GV_PLAYBACK)
		gviStopPlaybackDevice(data);
	else
		gviStopCaptureDevice(data);
}

static GVBool gviIsPlaybackDeviceStarted(GVIHardwareData * data)
{
	HRESULT result;
	DWORD status;

	result = IDirectSoundBuffer_GetStatus(data->m_playbackBuffer, &status);
	if(FAILED(result))
		return GVFalse;

	if(status & DSBSTATUS_PLAYING)
		return GVTrue;
	return GVFalse;
}

static GVBool gviIsCaptureDeviceStarted(GVIHardwareData * data)
{
	return data->m_capturing;
}

static GVBool gviHardwareIsDeviceStarted(GVIDevice * device, GVDeviceType type)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;

	if(type == GV_PLAYBACK)
		return gviIsPlaybackDeviceStarted(data);
	return gviIsCaptureDeviceStarted(data);
}

static void gviSetPlaybackDeviceVolume(GVIHardwareData * data, GVScalar volume)
{
	LONG vol;

	if(volume == 0.0)
		vol = -10000;
	else
		vol = (LONG)(log(volume) * 2000);

	IDirectSoundBuffer_SetVolume(data->m_playbackBuffer, vol);
}

static void gviSetCaptureDeviceVolume(GVIHardwareData * data, GVScalar volume)
{
	data->m_captureVolume = volume;
}

static void gviHardwareSetDeviceVolume(GVIDevice * device, GVDeviceType type, GVScalar volume)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;

	if(type == GV_PLAYBACK)
		gviSetPlaybackDeviceVolume(data, volume);
	else
		gviSetCaptureDeviceVolume(data, volume);
}

static GVScalar gviGetPlaybackDeviceVolume(GVIHardwareData * data)
{
	HRESULT result;
	LONG vol;

	result = IDirectSoundBuffer_GetVolume(data->m_playbackBuffer, &vol);
	if(FAILED(result))
		return (GVScalar)0;

	return (GVScalar)(exp(vol / 2000.0));
}

static GVScalar gviGetCaptureDeviceVolume(GVIHardwareData * data)
{
	return data->m_captureVolume;
}

static GVScalar gviHardwareGetDeviceVolume(GVIDevice * device, GVDeviceType type)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;

	if(type == GV_PLAYBACK)
		return gviGetPlaybackDeviceVolume(data);
	return gviGetCaptureDeviceVolume(data);
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
	HRESULT result;
	DWORD readPosition;
	DWORD newBytes;
	int numFrames;

	result = IDirectSoundCaptureBuffer_GetCurrentPosition(data->m_captureBuffer, NULL, &readPosition);
	if(FAILED(result))
	{
		gviDeviceUnplugged(device);
		return 0;
	}

	newBytes = (((readPosition + data->m_captureBufferSize) - data->m_captureBufferPosition) % data->m_captureBufferSize);

	numFrames = (newBytes / GVIBytesPerFrame);

	newBytes = (numFrames * GVIEncodedFrameSize);

	return newBytes;
}

static GVBool gviHardwareCapturePacket(GVDevice device, GVByte * packet, int * len, GVFrameStamp * frameStamp, GVScalar * volume)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;
	HRESULT result;
	DWORD readPosition;
	DWORD newBytes;
	LPVOID audioPtr1, audioPtr2;
	GVSample * audioPtr;
	DWORD audioLen, audioLen1, audioLen2;
	GVBool overThreshold = GVFalse;
	int numFrames;
	int i, j;
	int lenAvailable;
	int framesAvailable;

	lenAvailable = *len;

	*len = 0;
	if(volume)
		*volume = 0;

	result = IDirectSoundCaptureBuffer_GetCurrentPosition(data->m_captureBuffer, NULL, &readPosition);
	if(FAILED(result))
	{
		gviDeviceUnplugged(device);
		return GVFalse;
	}

	newBytes = (((readPosition + data->m_captureBufferSize) - data->m_captureBufferPosition) % data->m_captureBufferSize);

	numFrames = (newBytes / GVIBytesPerFrame);

	framesAvailable = (lenAvailable / GVIEncodedFrameSize);

	numFrames = min(numFrames, framesAvailable);
	if(!numFrames)
		return GVFalse;

	newBytes = (numFrames * GVIBytesPerFrame);

	if(data->m_capturing)
	{
		result = IDirectSoundCaptureBuffer_Lock(data->m_captureBuffer, data->m_captureBufferPosition, newBytes, &audioPtr1, &audioLen1, &audioPtr2, &audioLen2, 0);
		if(FAILED(result))
		{
			gviDeviceUnplugged(device);
			return GVFalse;
		}

		if(volume)
		{
			GVScalar vol1 = gviGetSamplesVolume(audioPtr1, audioLen1 / GV_BYTES_PER_SAMPLE);
			GVScalar vol2 = gviGetSamplesVolume(audioPtr2, audioLen2 / GV_BYTES_PER_SAMPLE);
			*volume = max(vol1, vol2);
		}

		if(volume)
		{
			overThreshold = (*volume >= data->m_captureThreshold);
		}
		else
		{
			overThreshold = gviIsOverThreshold(audioPtr1, audioLen1 / GV_BYTES_PER_SAMPLE, data->m_captureThreshold);

			if(!overThreshold && audioPtr2)
				overThreshold = gviIsOverThreshold(audioPtr2, audioLen2 / GV_BYTES_PER_SAMPLE, data->m_captureThreshold);
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

			audioPtr = audioPtr1;
			audioLen = audioLen1;

			for(i = 0 ; i < numFrames ; i++)
			{
				if(data->m_captureVolume < 1.0)
				{
					for(j = 0 ; j < GVISamplesPerFrame ; j++)
						audioPtr[j] = (GVSample)(audioPtr[j] * data->m_captureVolume);
				}

				if(device->m_captureFilterCallback)
					device->m_captureFilterCallback(device, audioPtr, (GVFrameStamp)(data->m_captureClock + i));

				gviEncode(packet + (GVIEncodedFrameSize * i), audioPtr);

				if(audioLen > (DWORD)GVIBytesPerFrame)
				{
					audioPtr += GVISamplesPerFrame;
					audioLen -= GVIBytesPerFrame;
				}
				else
				{
					audioPtr = (GVSample*)audioPtr2;
					audioLen = audioLen2;
				}
			}
		}

		result = IDirectSoundCaptureBuffer_Unlock(data->m_captureBuffer, audioPtr1, audioLen1, audioPtr2, audioLen2);
		if(FAILED(result))
		{
			gviDeviceUnplugged(device);
			return GVFalse;
		}
	}

	data->m_captureBufferPosition += newBytes;
	data->m_captureBufferPosition %= data->m_captureBufferSize;

	data->m_captureClock = data->m_captureClock + (GVFrameStamp)numFrames;

	*len = (numFrames * GVIEncodedFrameSize);

	if(!overThreshold)
		return GVFalse;

	return GVTrue;
}

static void gviHardwarePlayPacket(GVIDevice * device, const GVByte * packet, int len, GVSource source, GVFrameStamp frameStamp, GVBool mute)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;

	if(!data->m_playing)
		return;

	gviAddPacketToSourceList(data->m_playbackSources, packet, len, source, frameStamp, mute, data->m_playbackClock);
}

static GVBool gviHardwareIsSourceTalking(GVDevice device, GVSource source)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;

	if(!data->m_playing)
		return GVFalse;

	return gviIsSourceTalking(data->m_playbackSources, source);
}

static int gviHardwareListTalkingSources(GVDevice device, GVSource sources[], int maxSources)
{
	GVIHardwareData * data = (GVIHardwareData *)device->m_data;

	if(!data->m_playing)
		return GVFalse;

	return gviListTalkingSources(data->m_playbackSources, sources, maxSources);
}

static GVBool gviInitPlaybackDevice(GVIHardwareData * data)
{
	HRESULT result;
	DSBUFFERDESC bufferDescriptor;

	result = IDirectSound_SetCooperativeLevel(data->m_playback, GVIHwnd, DSSCL_NORMAL);
	if(FAILED(result))
		return GVFalse;

	data->m_playbackBufferSize = gviMultiplyByBytesPerMillisecond(GVI_PLAYBACK_BUFFER_MILLISECONDS);

	data->m_playbackBufferSize = gviRoundUpToNearestMultiple(data->m_playbackBufferSize, GVIBytesPerFrame * 2);

	data->m_playbackBufferHalfSize = (data->m_playbackBufferSize / 2);

	memset(&bufferDescriptor, 0, sizeof(DSBUFFERDESC));
	bufferDescriptor.dwSize = sizeof(DSBUFFERDESC);
	bufferDescriptor.dwFlags = DSBCAPS_CTRLVOLUME|DSBCAPS_GETCURRENTPOSITION2;
#if !defined(GV_NO_GLOBAL_FOCUS)
	bufferDescriptor.dwFlags |= DSBCAPS_GLOBALFOCUS;
#endif
	bufferDescriptor.dwBufferBytes = (DWORD)data->m_playbackBufferSize;
	bufferDescriptor.lpwfxFormat = &GVIWaveFormat;

	result = IDirectSound_CreateSoundBuffer(data->m_playback, &bufferDescriptor, &data->m_playbackBuffer, NULL);
	if(FAILED(result))
		return GVFalse;

	return GVTrue;
}

static GVBool gviInitCaptureDevice(GVIHardwareData * data)
{
	HRESULT result;
	DSCBUFFERDESC bufferDescriptor;

	data->m_captureBufferSize = gviMultiplyByBytesPerMillisecond(GVI_CAPTURE_BUFFER_MILLISECONDS);

	data->m_captureBufferSize = gviRoundUpToNearestMultiple(data->m_captureBufferSize, GVIBytesPerFrame);

	memset(&bufferDescriptor, 0, sizeof(bufferDescriptor));
	bufferDescriptor.dwSize = sizeof(DSCBUFFERDESC);
	bufferDescriptor.dwBufferBytes = (DWORD)data->m_captureBufferSize;
	bufferDescriptor.lpwfxFormat = &GVIWaveFormat;

	result = IDirectSoundCapture_CreateCaptureBuffer(data->m_capture, &bufferDescriptor, &data->m_captureBuffer, NULL);
	if(FAILED(result))
		return GVFalse;

	result = IDirectSoundCaptureBuffer_Start(data->m_captureBuffer, DSCBSTART_LOOPING);
	if(FAILED(result))
	{
		IDirectSoundCaptureBuffer_Release(data->m_captureBuffer);
		data->m_captureBuffer = NULL;
		return GVFalse;
	}

	return GVTrue;
}

GVDevice gviHardwareNewDevice(GVDeviceID deviceID, GVDeviceType type)
{
	GVIDevice * device;
	GVIHardwareData * data;
	HRESULT result;

	if((type != GV_CAPTURE) && (type != GV_PLAYBACK))
		return NULL;

	memset(&GVIWaveFormat, 0, sizeof(WAVEFORMATEX));
	GVIWaveFormat.wFormatTag = WAVE_FORMAT_PCM;
	GVIWaveFormat.nChannels = 1;
	GVIWaveFormat.nSamplesPerSec = GVISampleRate;
	GVIWaveFormat.wBitsPerSample = GV_BITS_PER_SAMPLE;
	GVIWaveFormat.nBlockAlign = ((GVIWaveFormat.nChannels * GVIWaveFormat.wBitsPerSample) / 8); 
	GVIWaveFormat.nAvgBytesPerSec = (GVIWaveFormat.nSamplesPerSec * GVIWaveFormat.nBlockAlign);


	device = gviNewDevice(deviceID, GVHardwareDirectSound, type, sizeof(GVIHardwareData));
	if(!device)
		return NULL;

	data = (GVIHardwareData *)device->m_data;

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

	if(type == GV_PLAYBACK)
	{
		data->m_playbackSources = gviNewSourceList();
		if(!data->m_playbackSources)
		{
			gviFreeDevice(device);
			return NULL;
		}

		result = DirectSoundCreate(&deviceID, &data->m_playback, NULL);
		if(FAILED(result))
		{
			gviFreeSourceList(data->m_playbackSources);
			gviFreeDevice(device);
			return NULL;
		}

		if(!gviInitPlaybackDevice(data))
		{
			gviFreeSourceList(data->m_playbackSources);
			IDirectSound_Release(data->m_playback);
			gviFreeDevice(device);
			return NULL;
		}
	}
	else if(type == GV_CAPTURE)
	{
		result = DirectSoundCaptureCreate(&deviceID, &data->m_capture, NULL);
		if(FAILED(result))
		{
			gviFreeDevice(device);
			return NULL;
		}

		if(!gviInitCaptureDevice(data))
		{
			IDirectSoundCapture_Release(data->m_capture);
			gviFreeDevice(device);
			return NULL;
		}

		data->m_captureVolume = 1.0;
		data->m_captureClock = 0;
		data->m_captureLastCrossedThresholdTime = (data->m_captureClock - GVI_HOLD_THRESHOLD_FRAMES - 1);
	}

	gviAppendDeviceToList(GVIDevices, device);

	return device;
}

#if (DIRECTSOUND_VERSION == 0x0800)
static LPDIRECTPLAYVOICETEST GetVoiceTestInstance(void)
{
	LPDIRECTPLAYVOICETEST voiceTest;
	HRESULT result;

	result = CoCreateInstance(&CLSID_DirectPlayVoiceTest, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectPlayVoiceTest, (LPVOID*)&voiceTest);

	if(FAILED(result))
		return NULL;

	return voiceTest;
}

GVBool gvRunSetupWizard(GVDeviceID captureDeviceID, GVDeviceID playbackDeviceID)
{
	LPDIRECTPLAYVOICETEST voiceTest;
	HRESULT result;

	voiceTest = GetVoiceTestInstance();
	if(!voiceTest)
		return GVFalse;

	result = IDirectPlayVoiceTest_CheckAudioSetup(voiceTest, &playbackDeviceID, &captureDeviceID, GVIHwnd, 0);

	IDirectPlayVoiceTest_Release(voiceTest);

	if((result == DV_OK) || (result == DV_FULLDUPLEX) || (result == DV_HALFDUPLEX))
		return GVTrue;

	return GVFalse;
}

GVBool gvAreDevicesSetup(GVDeviceID captureDevice, GVDeviceID playbackDevice)
{
	LPDIRECTPLAYVOICETEST voiceTest;
	HRESULT result;

	voiceTest = GetVoiceTestInstance();
	if(!voiceTest)
		return GVFalse;

	result = IDirectPlayVoiceTest_CheckAudioSetup(voiceTest, &playbackDevice, &captureDevice, GVIHwnd, DVFLAGS_QUERYONLY);

	IDirectPlayVoiceTest_Release(voiceTest);

	if((result == DV_FULLDUPLEX) || (result == DV_HALFDUPLEX))
		return GVTrue;

	return GVFalse;
}
#endif
