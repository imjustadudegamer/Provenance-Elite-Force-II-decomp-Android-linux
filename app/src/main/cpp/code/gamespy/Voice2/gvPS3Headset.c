#include "gvPS3Headset.h"
#if !defined(GV_NO_PS3_HEADSET)
#include "gvDevice.h"
#include "gvCodec.h"
#include "gvSource.h"
#include "gvUtil.h"
#include <types.h>
#include <sys/event.h>
#include <cell/audio.h>
#include <cell/mic.h>
#include <cell/sysmodule.h>
#include <cell/mixer.h>


#if !defined(_PS3)
#error This file should only be used with the PlayStation3
#endif

#define GVI_PLAYBACK_STAYAHEAD_MILLISECONDS  50

#define GVI_AUDIO_QUEUE_KEY_BASE	0x0000998877660000ULL

#define GVI_CAPTURE_QUEUE_KEY_BASE  0x0000000072110700UL 

#define GVI_AUDIO_QUEUE_DEPTH		4
#define GVI_CAPTURE_VOLUME_MAX      241
#define GVI_PLAYBACK_NUM_BLOCKS     CELL_AUDIO_BLOCK_32
#define GVI_PLAYBACK_BLOCK_SAMPLES  CELL_AUDIO_BLOCK_SAMPLES
#define GVI_PLAYBACK_SAMPLE_RATE    48000 
#define GVI_PS3_MIC_BUFFER_MS       1000
#define GVI_LOCAL_TALK_MAX          10
#define GVI_PLAYBACK_SAMPLE_FACTOR  (GVI_PLAYBACK_SAMPLE_RATE / gviGetSampleRate())
#define GVI_NUM_CHANNELS            CELL_AUDIO_PORT_2CH


typedef struct
{
	sys_event_t         m_captureCallbackEvent;
	sys_event_queue_t   m_captureCallbackQueue;
	uint64_t            m_captureEventQueueKey;
	GVBool              m_capturing;
	GVScalar            m_captureVolume;
	GVFrameStamp        m_captureClock;
	GVScalar            m_captureThreshold;
	GVFrameStamp        m_captureLastCrossedThresholdTime;
	float              *m_capturePreConvertBuffer;
	size_t              m_capturePreConvertBufferLen;
	GVSample           *m_captureBuffer;

	int                 m_captureBufferBytes;
	int                 m_deviceNum; 
	GVBool              m_captureMicOpen;

	GVBool              m_playing;
	GVScalar            m_playbackVolume;
	GVFrameStamp        m_playbackClock;
	GVISourceList       m_playbackSources;
	GVSample           *m_playbackBuffer;
	gsi_u32             m_playbackCellAudioPortNum;
	CellAudioPortConfig m_playbackCellAudioConfig;
	sys_event_queue_t   m_playbackQueue;
	uint64_t            m_playbackQueueKey;
	int                 m_playbackPortPos;
} GVIPS3HeadsetData;



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
	GS_ASSERT(elem);
	GVIDevice * device = *(GVIDevice **)elem;
	GVIPS3HeadsetData * data = (GVIPS3HeadsetData *)device->m_data;
	int result;

	if (data->m_capturing && cellMicIsOpen(data->m_deviceNum))
		result = cellMicClose(data->m_deviceNum);

	if (data->m_playbackQueue)
		cellAudioRemoveNotifyEventQueue(data->m_playbackQueueKey);
	if (data->m_captureCallbackQueue)
		cellMicRemoveNotifyEventQueue(data->m_captureEventQueueKey);
	if (data->m_playbackQueue)
		sys_event_queue_destroy(data->m_playbackQueue, 0);
	if (data->m_captureCallbackQueue)
		sys_event_queue_destroy(data->m_captureCallbackQueue, 0);

	if (data->m_playing)
		cellAudioPortClose(data->m_playbackCellAudioPortNum);

	if(device->m_types & GV_PLAYBACK)
	{
		if(data->m_playbackSources)
			gviFreeSourceList(data->m_playbackSources);
		gsifree(data->m_playbackBuffer);
	}

	if(device->m_types & GV_CAPTURE)
	{
		gsifree(data->m_captureBuffer);
		gsifree(data->m_capturePreConvertBuffer);
	}

	gviFreeDevice(device);
}


GVBool gviPS3HeadsetStartup(void)
{
	int result;

	GVIDevices = gviNewDeviceList(gviFreeArrayDevice);
	if(!GVIDevices)
		return GVFalse;

	result = cellSysmoduleLoadModule(CELL_SYSMODULE_MIC);
	if(result != CELL_OK)
	{
		gviFreeDeviceList(GVIDevices);
		GVIDevices = NULL;
		return GVFalse;
	}

	result = cellMicInit();
	if(result != CELL_OK)
	{
		gviFreeDeviceList(GVIDevices);
		GVIDevices = NULL;
		return GVFalse;
	}

	result = cellSysmoduleLoadModule(CELL_SYSMODULE_AUDIO);
	if(result != CELL_OK)
	{
		gviFreeDeviceList(GVIDevices);
		GVIDevices = NULL;
		return GVFalse;
	}

	result = cellAudioInit();
	if(result != CELL_OK && result != CELL_AUDIO_ERROR_ALREADY_INIT)
	{
		gviFreeDeviceList(GVIDevices);
		cellMicEnd();
		GVIDevices = NULL;
		return GVFalse;
	}

	return GVTrue;
}

void gviPS3HeadsetCleanup(void)
{
	if(GVIDevices)
	{
		gviFreeDeviceList(GVIDevices);
		GVIDevices = NULL;
	}

	cellMicEnd();
}


static GVBool gviPlaybackDeviceThink(GVIDevice * device)
{
	GVIPS3HeadsetData * data = (GVIPS3HeadsetData *)device->m_data;
	int remainingSamples;
	int numFrames;
	GVBool wroteToBuffer;
	int i, j, k;
	sys_event_t playbackQueueEvent;
	int result;
	int playbackReadPos;
	unsigned int currentBlock;
	int totalSamples;

	if(!data->m_playing)
		return GVTrue;

	result = sys_event_queue_receive(data->m_playbackQueue, &playbackQueueEvent, 1);
	if(result == ETIMEDOUT)
	{
		return GVTrue;
	}
	if(result != CELL_OK)
	{
		return GVFalse;
	}

	totalSamples = (GVI_PLAYBACK_NUM_BLOCKS * GVI_PLAYBACK_BLOCK_SAMPLES);

	currentBlock = (unsigned int)*(uint64_t *)(data->m_playbackCellAudioConfig.readIndexAddr);
	playbackReadPos = (currentBlock * GVI_PLAYBACK_BLOCK_SAMPLES);

	if(data->m_playbackPortPos == -1)
	{
		unsigned int nextBlock = (currentBlock + 1) % GVI_PLAYBACK_NUM_BLOCKS; 
		data->m_playbackPortPos = (nextBlock * GVI_PLAYBACK_BLOCK_SAMPLES);
	}

	remainingSamples = (((playbackReadPos + totalSamples) - data->m_playbackPortPos) % totalSamples);

	numFrames = ((remainingSamples / GVI_PLAYBACK_SAMPLE_FACTOR) / GVISamplesPerFrame);

	for(i = 0 ; i < numFrames ; i++)
	{
		wroteToBuffer = gviWriteSourcesToBuffer(data->m_playbackSources, data->m_playbackClock, data->m_playbackBuffer, 1);

		if(!wroteToBuffer)
			memset(data->m_playbackBuffer, 0, (unsigned int)GVIBytesPerFrame);

		if(device->m_playbackFilterCallback)
			device->m_playbackFilterCallback(device, data->m_playbackBuffer, data->m_playbackClock);
		
		for(j = 0 ; j < GVISamplesPerFrame ; j++)
		{
			float sample = ((float)data->m_playbackBuffer[j] / (float)SHRT_MAX);
			for(k = 0; k < GVI_PLAYBACK_SAMPLE_FACTOR; k++)
			{
				float *dest = (float *)(data->m_playbackCellAudioConfig.portAddr +
					(data->m_playbackPortPos * GVI_NUM_CHANNELS * sizeof(float)));
				*dest++ = sample;
				*dest = sample;
				data->m_playbackPortPos++;
				data->m_playbackPortPos %= totalSamples;
			}
		}

		data->m_playbackClock++;
	}

	return GVTrue;
}

void gviPS3HeadsetThink(void)
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

int gviPS3HeadsetListDevices(GVDeviceInfo devices[], int maxDevices, GVDeviceType types)
{
	int index;
	int numDevices = 0;

	maxDevices = min(maxDevices, CELL_MAX_MICS);

	for(index = 0 ; index < maxDevices ; index++)
	{
		if(cellMicIsAttached(index))
		{
			int deviceType;
			if (cellMicGetType(index, &deviceType) == CELL_OK)
			{
				if (deviceType == CELLMIC_TYPE_USBAUDIO)
				{
					devices[numDevices].m_id = index;
					devices[numDevices].m_deviceType = GV_CAPTURE | GV_PLAYBACK;
					devices[numDevices].m_defaultDevice = (GVDeviceType)0; 
					_tcscpy(devices[numDevices].m_name, _T("USB Headset"));
					devices[numDevices].m_hardwareType = GVHardwarePS3Headset;
				}
				else if (deviceType == CELLMIC_TYPE_BLUETOOTH)
				{
					devices[numDevices].m_id = index;
					devices[numDevices].m_deviceType = GV_CAPTURE | GV_PLAYBACK;
					devices[numDevices].m_defaultDevice = (GVDeviceType)0; 
					_tcscpy(devices[numDevices].m_name, _T("Bluetooth Headset"));
					devices[numDevices].m_hardwareType = GVHardwarePS3Headset;
				}
			}

			numDevices++;
		}
	}
	return numDevices;
}

static void gviPS3HeadsetFreeDevice(GVIDevice * device)
{
	gviDeleteDeviceFromList(GVIDevices, device);
}

static GVBool gviPS3HeadsetInitHeadphone(GVIPS3HeadsetData *data)
{
	int result;
	CellAudioPortParam  audioParam;
	sys_event_queue_attribute_t	aQueueAttr;
	int aCount;	

	audioParam.attr = CELL_AUDIO_PORTATTR_OUT_SECONDARY;
	audioParam.nBlock = GVI_PLAYBACK_NUM_BLOCKS;
	audioParam.nChannel = GVI_NUM_CHANNELS;

	data->m_playbackCellAudioPortNum = 0xFFFFFFFF;


	result = cellAudioPortOpen(&audioParam, &data->m_playbackCellAudioPortNum);
	if (result != CELL_OK)
	{	
		return GVFalse;
	}

	result = cellAudioGetPortConfig(data->m_playbackCellAudioPortNum, &data->m_playbackCellAudioConfig);
	if (result != CELL_OK)
	{
		return GVFalse;
	}

	aCount = 0;
	sys_event_queue_attribute_initialize(aQueueAttr);
	aQueueAttr.attr_protocol  = SYS_SYNC_FIFO;
	data->m_playbackQueueKey = GVI_AUDIO_QUEUE_KEY_BASE;

	while (aCount < 10)
	{
		result = sys_event_queue_create(&data->m_playbackQueue, &aQueueAttr, 
			data->m_playbackQueueKey, GVI_AUDIO_QUEUE_DEPTH);
		if (result == CELL_OK)
		{
			break;
		}
		data->m_playbackQueueKey = GVI_AUDIO_QUEUE_KEY_BASE | (rand() & 0x0ffff);
		aCount++;
	}

	if (result != CELL_OK)
	{
		return GVFalse;
	}

	result = cellAudioSetNotifyEventQueue(data->m_playbackQueueKey);
	if (result < 0)
	{
		return GVFalse;
	}

	return GVTrue;
}

GVBool gviPS3HeadsetInitMic(GVIPS3HeadsetData *data)
{
	int result = 0;
	int aMsg = 0;
	int aDevNum=0;

	int aCount = 0; 
	sys_event_queue_attribute_t  equeue_attr = {SYS_SYNC_FIFO, SYS_PPU_QUEUE, ""};

	data->m_captureEventQueueKey = GVI_CAPTURE_QUEUE_KEY_BASE;

	while ( (aCount++) < 100 )
	{
		result = sys_event_queue_create(&data->m_captureCallbackQueue, 
			&equeue_attr, data->m_captureEventQueueKey, 32);
		if (result == CELL_OK) break;
		data->m_captureEventQueueKey =  GVI_CAPTURE_QUEUE_KEY_BASE | ( rand() & 0xffff);
	}
	if (result != CELL_OK)
	{		
		return GVFalse;
	}

	result = cellMicSetNotifyEventQueue(data->m_captureEventQueueKey);
	if (result != CELL_OK)
	{
		return GVFalse;
	}

	result = sys_event_queue_receive(data->m_captureCallbackQueue, 
		&data->m_captureCallbackEvent, 10000);
	if(result == ETIMEDOUT)
		return GVFalse;

	aMsg = (int)data->m_captureCallbackEvent.data1;
	aDevNum = (int)data->m_captureCallbackEvent.data2;

	if (aMsg == CELLMIC_ATTACH && aDevNum == data->m_deviceNum)
	{
		result = cellMicOpenEx(data->m_deviceNum, GVI_PLAYBACK_SAMPLE_RATE, 1, 
			GV_SAMPLES_PER_SECOND, GVI_PS3_MIC_BUFFER_MS, CELLMIC_SIGTYPE_DSP);

		if (result != CELL_OK)
		{
			return GVFalse;
		}
		data->m_captureMicOpen = GVTrue;
	}
	return GVTrue;
}

void gviPS3HeadsetCloseHeadphone(GVIPS3HeadsetData *data)
{
	if (data->m_playbackQueue)
		cellAudioRemoveNotifyEventQueue(data->m_playbackQueue);
	if (data->m_playbackQueue)
		sys_event_queue_destroy(data->m_playbackQueue, 0);

	if (data->m_playbackCellAudioPortNum != 0xFFFFFFFF)
		cellAudioPortClose(data->m_playbackCellAudioPortNum);
}

void gviPS3HeadsetCloseMic(GVIPS3HeadsetData *data)
{
	if (data->m_captureCallbackQueue)
		cellAudioRemoveNotifyEventQueue(data->m_captureCallbackQueue);
	if (data->m_captureCallbackQueue)
		sys_event_queue_destroy(data->m_captureCallbackQueue, 0);

	if (cellMicIsOpen(data->m_deviceNum))
		cellMicClose(data->m_deviceNum);
}

static GVBool gviStartPlaybackDevice(GVIPS3HeadsetData * data)
{
	int result;

	sys_event_queue_drain(data->m_playbackQueue);

	data->m_playbackPortPos = -1;

	result = cellAudioPortStart(data->m_playbackCellAudioPortNum);
	if (result != CELL_OK)
	{
		return GVFalse;
	}

	data->m_playbackClock = 0;

	data->m_playing = GVTrue;

	return GVTrue;
}

static GVBool gviStartCaptureDevice(GVIPS3HeadsetData * data)
{
	int result;
	if (data->m_captureMicOpen)
	{		
		result = cellMicStart(data->m_deviceNum);
		if (result != CELL_OK)
			return GVFalse;
	}
	else 
		return GVFalse;

	cellMicReset(data->m_deviceNum);
	data->m_captureBufferBytes = 0;
	data->m_capturePreConvertBufferLen = 0;

	data->m_capturing = GVTrue;

	return GVTrue;
}

static void gviPS3HeadsetStopDevice(GVIDevice * device, GVDeviceType type)
{
	GVIPS3HeadsetData * data = (GVIPS3HeadsetData *)device->m_data;

	if(type & GV_PLAYBACK)
	{
		cellAudioPortStop(data->m_playbackCellAudioPortNum);

		memset(data->m_playbackBuffer, 0, GVIBytesPerFrame);

		data->m_playing = GVFalse;

		gviClearSourceList(data->m_playbackSources);
	}
	if(type & GV_CAPTURE)
	{
		cellMicStop(data->m_deviceNum);

		memset(data->m_captureBuffer, 0, GVIBytesPerFrame);

		data->m_capturing = GVFalse;

		data->m_captureClock++;
	}

}

static GVBool gviPS3HeadsetStartDevice(GVIDevice * device, GVDeviceType type)
{
	GVIPS3HeadsetData * data = (GVIPS3HeadsetData *)device->m_data;

	if(type == GV_PLAYBACK)
	{
		return gviStartPlaybackDevice(data);
	}
	if(type == GV_CAPTURE)
	{
		return gviStartCaptureDevice(data);
	}
	if(type == GV_CAPTURE_AND_PLAYBACK)
	{
		if(!gviStartPlaybackDevice(data))
			return GVFalse;
		if(!gviStartCaptureDevice(data))
		{
			gviPS3HeadsetStopDevice(device, GV_PLAYBACK);
			return GVFalse;
		}
		return GVTrue;
	}
	return GVFalse;
}

static GVBool gviPS3HeadsetIsDeviceStarted(GVIDevice * device, GVDeviceType type)
{
	GS_ASSERT(device);
	if (!device)
		return GVFalse;

	GVIPS3HeadsetData * data = (GVIPS3HeadsetData *)device->m_data;

	if(type == GV_PLAYBACK)
		return data->m_playing;
	if(type == GV_CAPTURE)
		return data->m_capturing;
	if(type == GV_CAPTURE_AND_PLAYBACK)
		return (data->m_playing && data->m_capturing);
	return GVFalse;
}

static void gviPS3HeadsetSetDeviceVolume(GVIDevice * device, GVDeviceType type, GVScalar volume)
{
	GVIPS3HeadsetData * data = (GVIPS3HeadsetData *)device->m_data;

	if(type & GV_PLAYBACK)
	{
		cellAudioSetPortLevel(data->m_playbackCellAudioPortNum, volume);
		data->m_playbackVolume = volume;
	}
	if(type & GV_CAPTURE)
	{
		cellMicSetDeviceAttr(data->m_deviceNum, CELLMIC_DEVATTR_VOLUME, (int)(volume * GVI_CAPTURE_VOLUME_MAX), 0);
		data->m_captureVolume = volume;
	}
}

static GVScalar gviPS3HeadsetGetDeviceVolume(GVIDevice * device, GVDeviceType type)
{
	GVIPS3HeadsetData * data = (GVIPS3HeadsetData *)device->m_data;

	if(type & GV_PLAYBACK)
		return data->m_playbackVolume;
	return data->m_captureVolume;
}

static void gviPS3HeadsetSetCaptureThreshold(GVIDevice * device, GVScalar threshold)
{
	GVIPS3HeadsetData * data = (GVIPS3HeadsetData *)device->m_data;
	data->m_captureThreshold = threshold;
}

static GVScalar gviPS3HeadsetGetCaptureThreshold(GVIDevice * device)
{
	GVIPS3HeadsetData * data = (GVIPS3HeadsetData *)device->m_data;
	return data->m_captureThreshold;
}

static int gviPS3HeadsetGetAvailableCaptureBytes(GVDevice device)
{
	GVIPS3HeadsetData * data = (GVIPS3HeadsetData *)device->m_data;

	if(!data->m_capturing)
		return 0;

	return 1;
}

GVBool gviPS3HeadsetHandleMicAttach(GVIPS3HeadsetData *data)
{
	int result;

	result = cellMicOpenEx(data->m_deviceNum, GVI_PLAYBACK_SAMPLE_RATE, 1, 
		GV_SAMPLES_PER_SECOND, GVI_PS3_MIC_BUFFER_MS, CELLMIC_SIGTYPE_DSP);

	if (result != CELL_OK)
	{
		return GVFalse;
	}
	data->m_captureMicOpen = GVTrue;
	result = cellMicStart(data->m_deviceNum);
	if (result != CELL_OK)
	{
		return GVFalse;
	}
	result = cellMicReset(data->m_deviceNum);
	if (result != CELL_OK)
	{
		return GVFalse;
	}
	return GVTrue;
}

static void gviProcessCapturedFrame(GVDevice device, GVSample *frameIn, GVByte* frameOut, GVScalar *volume, GVBool *threshold)
{
	GVIPS3HeadsetData * data = (GVIPS3HeadsetData *)device->m_data;
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

static GVBool gviPS3HeadsetCapturePacket(GVDevice device, GVByte * packet, int * len, GVFrameStamp * frameStamp, GVScalar * volume)
{
	GVIPS3HeadsetData * data = (GVIPS3HeadsetData *)device->m_data;
	GVBool overThreshold;
	int result;
	int numFrames;
	int readSize;
	int lenAvailable;
	int framesAvailable;
	GVByte * frameOut;
	float *frameIn;
	int aCaptureQueueMsg;
	int aDeviceIndex;
	int sample;
	int localtalk;
	lenAvailable = *len;

	*len = 0;
	if(volume)
		*volume = 0;

	if(!data->m_capturing)
		return GVFalse;

	*frameStamp = data->m_captureClock;

	framesAvailable = (lenAvailable / GVIEncodedFrameSize);
	overThreshold = GVFalse;

	frameOut = packet;
	for(numFrames = 0 ; numFrames < framesAvailable ; numFrames++)
	{
		result = sys_event_queue_receive(data->m_captureCallbackQueue, 
			&data->m_captureCallbackEvent, 1);
		if(result == ETIMEDOUT)
		{
			break;
		}	

		aCaptureQueueMsg = (int)data->m_captureCallbackEvent.data1;
		aDeviceIndex = (int)data->m_captureCallbackEvent.data2;
		if (aDeviceIndex != data->m_deviceNum)
			continue;

		if (aCaptureQueueMsg == CELLMIC_ATTACH)
		{
			if (!gviPS3HeadsetHandleMicAttach(data))
			{
				break;
			}
		}	
		else if (aCaptureQueueMsg == CELLMIC_DETACH)
		{
			gviDeviceUnplugged(device);
			return GVFalse;
		}
		else if (aCaptureQueueMsg == CELLMIC_DATA)
		{
			readSize = ((GVISamplesPerFrame - data->m_capturePreConvertBufferLen) * sizeof(float));
			frameIn = data->m_capturePreConvertBuffer + data->m_capturePreConvertBufferLen;
			readSize = cellMicRead(data->m_deviceNum, frameIn, readSize);

			cellMicGetSignalState(0, CELLMIC_SIGSTATE_LOCTALK, &localtalk);
			if (localtalk < (int)(GVI_LOCAL_TALK_MAX * data->m_captureThreshold))
			{
				return GVFalse;
			}

			if (readSize == CELL_MICIN_ERROR_DEVICE_NOT_FOUND)
			{
				gviDeviceUnplugged(device);
				return GVFalse;
			}

			data->m_capturePreConvertBufferLen += (readSize / sizeof(float));
			if(data->m_capturePreConvertBufferLen < GVISamplesPerFrame)
			{
				break;
			}

			for (sample = 0; sample < GVISamplesPerFrame; sample++)
			{
				data->m_captureBuffer[sample] = (GVSample)((SHRT_MAX)*data->m_capturePreConvertBuffer[sample]); 
			}

			gviProcessCapturedFrame(device, data->m_captureBuffer, frameOut, volume, &overThreshold);

			data->m_captureBufferBytes = 0;

			frameOut += GVIEncodedFrameSize;

			data->m_capturePreConvertBufferLen = 0;
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

static void gviPS3HeadsetPlayPacket(GVIDevice * device, const GVByte * packet, int len, GVSource source, GVFrameStamp frameStamp, GVBool mute)
{
	GVIPS3HeadsetData * data = (GVIPS3HeadsetData *)device->m_data;

	if(!data->m_playing)
		return;

	gviAddPacketToSourceList(data->m_playbackSources, packet, len, source, frameStamp, mute, data->m_playbackClock);
}

static GVBool gviPS3HeadsetIsSourceTalking(GVDevice device, GVSource source)
{
	GVIPS3HeadsetData * data = (GVIPS3HeadsetData *)device->m_data;

	if(!data->m_playing)
		return GVFalse;

	return gviIsSourceTalking(data->m_playbackSources, source);
}

static int gviPS3HeadsetListTalkingSources(GVDevice device, GVSource sources[], int maxSources)
{
	GVIPS3HeadsetData * data = (GVIPS3HeadsetData *)device->m_data;

	if(!data->m_playing)
		return GVFalse;

	return gviListTalkingSources(data->m_playbackSources, sources, maxSources);
}

static GVBool gviPS3HeadsetInitDevice(GVIDevice * device, int deviceIndex, GVDeviceType type)
{
	GVIPS3HeadsetData * data;

	data = (GVIPS3HeadsetData *)device->m_data;
	data->m_deviceNum = deviceIndex;

	if(type & GV_PLAYBACK)
	{
		data->m_playbackSources = gviNewSourceList();
		if(!data->m_playbackSources)
		{
			return NULL;
		}

		data->m_playbackBuffer = (GVSample *)gsimemalign(16, (unsigned int)(GVIBytesPerFrame));
		if(!data->m_playbackBuffer)
		{
			gviFreeSourceList(data->m_playbackSources);
			return NULL;
		}
		if (!gviPS3HeadsetInitHeadphone(data))
		{
			gviFreeSourceList(data->m_playbackSources);
			gsifree(data->m_playbackBuffer);
			gviPS3HeadsetCloseHeadphone(data);
			return NULL;
		}
		data->m_playbackVolume = 1.0;
	}

	if(type & GV_CAPTURE)
	{
		data->m_captureClock = 0;
		data->m_captureVolume = 1.0;
		data->m_capturePreConvertBufferLen = 0;

		data->m_captureLastCrossedThresholdTime = (GVFrameStamp)(data->m_captureClock - GVI_HOLD_THRESHOLD_FRAMES - 1);

		data->m_captureBuffer = (GVSample *)gsimemalign(16, (unsigned int)(GVIBytesPerFrame));
		if(!data->m_captureBuffer)
		{
			if(type & GV_PLAYBACK)
			{
				gviFreeSourceList(data->m_playbackSources);
				gsifree(data->m_playbackBuffer);
				gviPS3HeadsetCloseHeadphone(data);
			}
			return NULL;
		}

		data->m_capturePreConvertBuffer = (float *)gsimemalign(16,(unsigned int)(GVISamplesPerFrame * sizeof(float)));
		if(!data->m_capturePreConvertBuffer)
		{
			if(type & GV_PLAYBACK)
			{
				gviFreeSourceList(data->m_playbackSources);
				gsifree(data->m_playbackBuffer);
				gviPS3HeadsetCloseHeadphone(data);
			}

			gsifree(data->m_captureBuffer);
			return NULL;
		}

		if (!gviPS3HeadsetInitMic(data))
		{
			if(type & GV_PLAYBACK)
			{
				gviFreeSourceList(data->m_playbackSources);
				gsifree(data->m_playbackBuffer);
				gviPS3HeadsetCloseHeadphone(data);
			}
			gviPS3HeadsetCloseMic(data);
			gsifree(data->m_captureBuffer);
			gsifree(data->m_capturePreConvertBuffer);
			return NULL;
		}
	}

	return GVTrue;
}

GVDevice gviPS3HeadsetNewDevice(GVDeviceID deviceID, GVDeviceType type)
{
	GVIDevice * device;
	GVBool result;

	if(!(type & GV_CAPTURE_AND_PLAYBACK))
		return NULL;

	if(gviFindDeviceByID(deviceID))
		return NULL;

	device = gviNewDevice(deviceID, GVHardwarePS3Headset, type, sizeof(GVIPS3HeadsetData));
	if(!device)
		return NULL;

	result = gviPS3HeadsetInitDevice(device, deviceID, type);
	if(result == GVFalse)
	{
		gviFreeDevice(device);
		return NULL;
	}

	device->m_methods.m_freeDevice = gviPS3HeadsetFreeDevice;
	device->m_methods.m_startDevice = gviPS3HeadsetStartDevice;
	device->m_methods.m_stopDevice = gviPS3HeadsetStopDevice;
	device->m_methods.m_isDeviceStarted = gviPS3HeadsetIsDeviceStarted;
	device->m_methods.m_setDeviceVolume = gviPS3HeadsetSetDeviceVolume;
	device->m_methods.m_getDeviceVolume = gviPS3HeadsetGetDeviceVolume;
	device->m_methods.m_setCaptureThreshold = gviPS3HeadsetSetCaptureThreshold;
	device->m_methods.m_getCaptureThreshold = gviPS3HeadsetGetCaptureThreshold;
	device->m_methods.m_getAvailableCaptureBytes = gviPS3HeadsetGetAvailableCaptureBytes;
	device->m_methods.m_capturePacket = gviPS3HeadsetCapturePacket;
	device->m_methods.m_playPacket = gviPS3HeadsetPlayPacket;
	device->m_methods.m_isSourceTalking = gviPS3HeadsetIsSourceTalking;
	device->m_methods.m_listTalkingSources = gviPS3HeadsetListTalkingSources;

	gviAppendDeviceToList(GVIDevices, device);
	return device;
}

#endif 
