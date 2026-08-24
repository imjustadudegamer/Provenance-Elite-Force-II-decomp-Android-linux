#include "gvCustomDevice.h"
#include "gvSource.h"
#include "gvCodec.h"
#include "gvUtil.h"

#define GVI_DEFAULT_THRESHOLD   ((GVScalar)0.0)

#if defined(_WIN32)
static GVDeviceID GVICustomDeviceID = {0};
#else
static GVDeviceID GVICustomDeviceID = 0;
#endif 

typedef struct
{
	GVBool        m_captureStarted;
	GVScalar      m_captureThreshold;
	GVFrameStamp  m_captureClock;
	GVFrameStamp  m_captureLastCrossedThresholdTime;
	GVScalar      m_captureVolume;

	GVBool        m_playbackStarted;
	GVISourceList m_playbackSources;
	GVFrameStamp  m_playbackClock;
	GVScalar      m_playbackVolume;
} GVICustomData;

static void gviCustomFreeDevice(GVIDevice * device)
{
	GVICustomData * data = (GVICustomData *)device->m_data;

	if(device->m_types & GV_PLAYBACK)
	{
		data->m_playbackStarted = GVFalse;
		gviFreeSourceList(data->m_playbackSources);
	}
	if(device->m_types & GV_CAPTURE)
	{
		data->m_captureStarted = GVFalse;
	}

	gviFreeDevice(device);
}

static GVBool gviCustomStartDevice(GVIDevice * device, GVDeviceType type)
{
	GVICustomData * data = (GVICustomData *)device->m_data;

	if(type & GV_PLAYBACK)
	{
		data->m_playbackStarted = GVTrue;
		data->m_playbackClock = 0;
	}
	if(type & GV_CAPTURE)
	{
		data->m_captureStarted = GVTrue;
	}
	return GVTrue;
}

static void gviCustomStopDevice(GVIDevice * device, GVDeviceType type)
{
	GVICustomData * data = (GVICustomData *)device->m_data;

	if(type & GV_PLAYBACK)
	{
		data->m_playbackStarted = GVFalse;
		gviClearSourceList(data->m_playbackSources);
	}
	if(type & GV_CAPTURE)
	{
		data->m_captureStarted = GVFalse;
		data->m_captureClock++;
	}
}

static GVBool gviCustomIsDeviceStarted(GVIDevice * device, GVDeviceType type)
{
	GVICustomData * data = (GVICustomData *)device->m_data;

	if(type == GV_PLAYBACK)
		return data->m_playbackStarted;
	else if(type == GV_CAPTURE)
		return data->m_captureStarted;
	return (data->m_playbackStarted && data->m_captureStarted);
}

static void gviCustomSetDeviceVolume(GVIDevice * device, GVDeviceType type, GVScalar volume)
{
	GVICustomData * data = (GVICustomData *)device->m_data;

	if(type & GV_PLAYBACK)
		data->m_playbackVolume = volume;
	if(type & GV_CAPTURE)
		data->m_captureVolume = volume;
}

static GVScalar gviCustomGetDeviceVolume(GVIDevice * device, GVDeviceType type)
{
	GVICustomData * data = (GVICustomData *)device->m_data;

	if(type & GV_PLAYBACK)
		return data->m_playbackVolume;
	return data->m_captureVolume;
}

static void gviCustomSetCaptureThreshold(GVIDevice * device, GVScalar threshold)
{
	GVICustomData * data = (GVICustomData *)device->m_data;

	data->m_captureThreshold = threshold;
}

static GVScalar gviCustomGetCaptureThreshold(GVIDevice * device)
{
	GVICustomData * data = (GVICustomData *)device->m_data;

	return data->m_captureThreshold;
}

static int gviCustomGetAvailableCaptureBytes(GVDevice device)
{
	assert(0);
	GSI_UNUSED(device);
	return 0;
}

static GVBool gviCustomCapturePacket(GVDevice device, GVByte * packet, int * len, GVFrameStamp * frameStamp, GVScalar * volume)
{
	assert(0);
	GSI_UNUSED(device);
	GSI_UNUSED(packet);
	GSI_UNUSED(len);
	GSI_UNUSED(frameStamp);
	GSI_UNUSED(volume);
	return GVFalse;
}

static void gviCustomPlayPacket(GVIDevice * device, const GVByte * packet, int len, GVSource source, GVFrameStamp frameStamp, GVBool mute)
{
	GVICustomData * data = (GVICustomData *)device->m_data;

	if(!data->m_playbackStarted)
		return;

	gviAddPacketToSourceList(data->m_playbackSources, packet, len, source, frameStamp, mute, data->m_playbackClock);
}

static GVBool gviCustomIsSourceTalking(GVDevice device, GVSource source)
{
	GVICustomData * data = (GVICustomData *)device->m_data;

	if(!data->m_playbackStarted)
		return GVFalse;

	return gviIsSourceTalking(data->m_playbackSources, source);
}

static int gviCustomListTalkingSources(GVDevice device, GVSource sources[], int maxSources)
{
	GVICustomData * data = (GVICustomData *)device->m_data;

	if(!data->m_playbackStarted)
		return GVFalse;

	return gviListTalkingSources(data->m_playbackSources, sources, maxSources);
}

GVDevice gviCustomNewDevice(GVDeviceType type)
{
	GVIDevice * device;
	GVICustomData * data;

	device = gviNewDevice(GVICustomDeviceID, GVHardwareCustom, type, sizeof(GVICustomData));
	if(!device)
		return NULL;

	data = (GVICustomData *)device->m_data;

	device->m_methods.m_freeDevice = gviCustomFreeDevice;
	device->m_methods.m_startDevice = gviCustomStartDevice;
	device->m_methods.m_stopDevice = gviCustomStopDevice;
	device->m_methods.m_isDeviceStarted = gviCustomIsDeviceStarted;
	device->m_methods.m_setDeviceVolume = gviCustomSetDeviceVolume;
	device->m_methods.m_getDeviceVolume = gviCustomGetDeviceVolume;
	device->m_methods.m_setCaptureThreshold = gviCustomSetCaptureThreshold;
	device->m_methods.m_getCaptureThreshold = gviCustomGetCaptureThreshold;
	device->m_methods.m_getAvailableCaptureBytes = gviCustomGetAvailableCaptureBytes;
	device->m_methods.m_capturePacket = gviCustomCapturePacket;
	device->m_methods.m_playPacket = gviCustomPlayPacket;
	device->m_methods.m_isSourceTalking = gviCustomIsSourceTalking;
	device->m_methods.m_listTalkingSources = gviCustomListTalkingSources;

	if(type & GV_PLAYBACK)
	{
		data->m_playbackSources = gviNewSourceList();
		if(!data->m_playbackSources)
			return GVFalse;

		data->m_playbackStarted = GVFalse;
		data->m_playbackClock = 0;
		data->m_playbackVolume = 1.0;
	}
	if(type & GV_CAPTURE)
	{
		data->m_captureStarted = GVFalse;
		data->m_captureThreshold = GVI_DEFAULT_THRESHOLD;
		data->m_captureClock = 0;
		data->m_captureLastCrossedThresholdTime = (GVFrameStamp)(data->m_captureClock - GVI_HOLD_THRESHOLD_FRAMES - 1);
		data->m_captureVolume = 1.0;
	}

	return device;
}

GVBool gviGetCustomPlaybackAudio(GVIDevice * device, GVSample * audio, int numSamples)
{
	GVICustomData * data = (GVICustomData *)device->m_data;
	GVBool wroteToBuffer;
	int numFrames;
	int i;
	GVBool result = GVFalse;

	if(!data->m_playbackStarted)
		return GVFalse;

	assert(!(numSamples % GVISamplesPerFrame));

	numFrames = (numSamples / GVISamplesPerFrame);

	wroteToBuffer = gviWriteSourcesToBuffer(data->m_playbackSources, data->m_playbackClock,
                                            audio, numFrames);

	if(!wroteToBuffer)
	{
		memset(audio, 0, numSamples * GV_BYTES_PER_SAMPLE);
	}
	else
	{
		if(data->m_playbackVolume != 1.0)
		{
			for(i = 0 ; i < numSamples ; i++)
				audio[i] = (GVSample)(audio[i] * data->m_playbackVolume);
		}
		result = GVTrue;
	}

	if(device->m_playbackFilterCallback)
	{
		for(i = 0 ; i < numFrames ; i++)
			device->m_playbackFilterCallback(device, audio + (GVISamplesPerFrame * i), (GVFrameStamp)(data->m_playbackClock + i));
	}

	data->m_playbackClock = data->m_playbackClock + (GVFrameStamp)numFrames;

	return result;
}

GVBool gviSetCustomCaptureAudio(GVDevice device, const GVSample * audio, int numSamples,
                               GVByte * packet, int * packetLen, GVFrameStamp * frameStamp, GVScalar * volume)
{
	GVICustomData * data = (GVICustomData *)device->m_data;
	int numBytes;
	GVBool overThreshold = GVFalse;
	int numFrames;
	int i;
	int len;

	len = *packetLen;

	*packetLen = 0;
	if(volume)
		*volume = 0;

	if(!data->m_captureStarted)
		return GVFalse;

	assert(!(numSamples % GVISamplesPerFrame));

	*frameStamp = data->m_captureClock;

	numFrames = (numSamples / GVISamplesPerFrame);

	if(audio)
	{
		numBytes = (int)(numSamples * GV_BYTES_PER_SAMPLE);

		assert(len >= numBytes);
		if(len < numBytes)
			return GVFalse;

		if(volume)
			*volume = gviGetSamplesVolume(audio, numSamples);

		if(volume)
		{
			overThreshold = (*volume >= data->m_captureThreshold);
		}
		else
		{
			overThreshold = gviIsOverThreshold(audio, numSamples, data->m_captureThreshold);
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
			if(data->m_captureVolume != 1.0)
			{
				for(i = 0 ; i < numSamples ; i++)
					((GVSample *)audio)[i] = (GVSample)(audio[i] * data->m_captureVolume);
			}

			for(i = 0 ; i < numFrames ; i++)
			{
				if(device->m_captureFilterCallback)
					device->m_captureFilterCallback(device, (GVSample *)audio + (i * GVISamplesPerFrame), (GVFrameStamp)(data->m_captureClock + i));

				gviEncode(packet + (i * GVIEncodedFrameSize), audio + (i * GVISamplesPerFrame));
			}
		}

		*packetLen = (numFrames * GVIEncodedFrameSize);
	}

	data->m_captureClock = data->m_captureClock + (GVFrameStamp)numFrames;

	if(!overThreshold)
		return GVFalse;

	return GVTrue;
}
