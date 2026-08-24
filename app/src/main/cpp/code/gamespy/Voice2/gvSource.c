#include "gvSource.h"
#include "gvCodec.h"
#include "gvFrame.h"
#include "gvUtil.h"
#include "gvMain.h"
#include <stdio.h>


#define GVI_SYNCHRONIZATION_DELAY                 200

#define GVI_MAX_SOURCES                           8

#define GVI_CLOCK_OFFSET_AVERAGING_FACTOR         0.96875  

#define GVI_DIVERGENCE_HIGH_WATERMARK             10
#define GVI_DIVERGENCE_LOW_WATERMARK              ((GVFrameStamp)-5)

#define GVI_SHOW_SKEW_CORRECTIONS                 0
#define GVI_SHOW_SOURCELIST_CHANGES               0

typedef struct GVISource
{
	GVBool          m_inUse;
	GVSource        m_source;
	GVBool          m_isTalking;
	GVFrameStamp    m_finishedTalkingTime;
	GVFrameStamp    m_clockOffset;
	float           m_clockOffsetAverage;
	GVDecoderData   m_decoderData;
	GVIPendingFrame m_frameHead;
} GVISource;

static int GVISynchronizationDelayFrames;
GVBool     GVIGlobalMute;


static void gviFreeSource(GVISource * source)
{
	GVIPendingFrame * frame = source->m_frameHead.m_next;
	GVIPendingFrame * next;

	if(!source->m_inUse)
		return;

	while(frame)
	{
		next = frame->m_next;
		gviPutPendingFrame(frame);
		frame = next;
	}

	gviFreeDecoder(source->m_decoderData);

#if GVI_SHOW_SOURCELIST_CHANGES
	printf("Freed source\n");
#endif

	source->m_inUse = GVFalse;
}

GVISourceList gviNewSourceList(void)
{
	int size;
	GVISourceList sourceList;

	if(!GVISynchronizationDelayFrames)
	{
		int delay;
		
		delay = gviMultiplyByBytesPerMillisecond(GVI_SYNCHRONIZATION_DELAY);
		delay = gviRoundUpToNearestMultiple(delay, GVIBytesPerFrame);

		GVISynchronizationDelayFrames = (delay / GVIBytesPerFrame);
	}

	size = (sizeof(GVISource) * GVI_MAX_SOURCES);
	sourceList = (GVISourceList)gsimalloc((unsigned int)size);
	if(sourceList)
		memset(sourceList, 0, (unsigned int)size);

	return sourceList;
}

void gviFreeSourceList(GVISourceList sourceList)
{
	gviClearSourceList(sourceList);
	gsifree(sourceList);
}

void gviClearSourceList(GVISourceList sourceList)
{
	int i;

	assert(sourceList);

	for(i = 0 ; i < GVI_MAX_SOURCES ; i++)
		gviFreeSource(&sourceList[i]);
}

static GVISource * gviFindSourceInList(GVISourceList sourceList, GVSource source)
{
	GVISource * gviSource;
	int i;

	for(i = 0 ; i < GVI_MAX_SOURCES ; i++)
	{
		gviSource = &sourceList[i];
		if(gviSource->m_inUse)
		{
			if(memcmp(&gviSource->m_source, &source, sizeof(GVSource)) == 0)
			{
				return gviSource;
			}
		}
	}

	return NULL;
}

GVBool gviIsSourceTalking(GVISourceList sourceList, GVSource source)
{
	GVISource * gviSource = gviFindSourceInList(sourceList, source);
	if(!gviSource)
		return GVFalse;

	return gviSource->m_isTalking;
}

int gviListTalkingSources(GVISourceList sourceList, GVSource sources[], int maxSources)
{
	GVISource * gviSource;
	int numTalking = 0;
	int i;

	for(i = 0 ; i < GVI_MAX_SOURCES ; i++)
	{
		gviSource = &sourceList[i];

		if(gviSource->m_inUse && gviSource->m_isTalking)
		{
			memcpy(&sources[numTalking], &gviSource->m_source, sizeof(GVSource));

			numTalking++;

			if(numTalking == maxSources)
				break;
		}
	}

	return numTalking;
}

void gviSetGlobalMute(GVBool mute)

{
	GVIGlobalMute = mute;
}

GVBool gviGetGlobalMute(void)
{
	return GVIGlobalMute;
}

static GVISource * gviAddSourceToList(GVISourceList sourceList, GVSource source)
{
	GVBool result;
	GVISource * gviSource = NULL;
	int i;

	for(i = 0 ; i < GVI_MAX_SOURCES ; i++)
	{
		if(!sourceList[i].m_inUse)
		{
			gviSource = &sourceList[i];
			break;
		}

		if(!gviSource && !sourceList[i].m_frameHead.m_next)
			gviSource = &sourceList[i];
	}

	if(!gviSource)
		return NULL;

	if(gviSource->m_inUse)
		gviFreeSource(gviSource);

	result = gviNewDecoder(&gviSource->m_decoderData);
	if(!result)
		return NULL;

	gviSource->m_inUse = GVTrue;

	memcpy(&gviSource->m_source, &source, sizeof(GVSource));
	gviSource->m_clockOffset = 0;
	gviSource->m_clockOffsetAverage = 0;
	gviSource->m_isTalking = GVFalse;
	gviSource->m_finishedTalkingTime = 0;
	gviSource->m_frameHead.m_next = NULL;

#if GVI_SHOW_SOURCELIST_CHANGES
	printf("Added source\n");
#endif

	return gviSource;
}

static void gviAddPacketToSource(GVISource * source, GVFrameStamp frameStamp, const GVByte * packet, int len, GVBool mute)
{
	GVIPendingFrame * frame;
	GVIPendingFrame * nextFrame;
	GVIPendingFrame * newFrame;
	GVFrameStamp packetFinishedTime;
	int numFrames;
	int i;

	assert(!(len % GVIEncodedFrameSize));

	numFrames = (len / GVIEncodedFrameSize);

	frameStamp = frameStamp + (GVFrameStamp)source->m_clockOffset;

	packetFinishedTime = (GVFrameStamp)(frameStamp + numFrames);

	if(gviIsFrameStampGT(packetFinishedTime, source->m_finishedTalkingTime))
		source->m_finishedTalkingTime = packetFinishedTime;

	if(mute || GVIGlobalMute)
	{
		source->m_isTalking = GVTrue;
		return;
	}

	for(frame = &source->m_frameHead ; frame->m_next ; frame = frame->m_next)
	{
		nextFrame = frame->m_next;

		if(nextFrame->m_frameStamp == frameStamp)
			return;

		if(gviIsFrameStampGT(nextFrame->m_frameStamp, frameStamp))
		{
			assert(!gviIsFrameStampGT(packetFinishedTime, nextFrame->m_frameStamp));

			break;
		}
	}

	for(i = 0 ; i < numFrames ; i++)
	{
		newFrame = gviGetPendingFrame();
		if(!newFrame)
			return;

		newFrame->m_frameStamp = frameStamp;
#if GVI_PRE_DECODE
		gviDecodeSet(newFrame->m_frame, packet, source->m_decoderData);
#else
		memcpy(newFrame->m_frame, packet, (unsigned int)GVIEncodedFrameSize);
#endif
		newFrame->m_next = frame->m_next;

		frame->m_next = newFrame;

		frameStamp++;
		packet += GVIEncodedFrameSize;
		frame = newFrame;
	}
}

void gviAddPacketToSourceList(GVISourceList sourceList,
							  const GVByte * packet, int len, GVSource source, GVFrameStamp frameStamp, GVBool mute,
							  GVFrameStamp currentPlayClock)
{
	GVFrameStamp playTime;
	GVISource * gviSource;
	GVFrameStamp clockOffset;
	GVFrameStamp divergence;
	int unwrappedClockOffset;

	playTime = (GVFrameStamp)(currentPlayClock + GVISynchronizationDelayFrames);

	clockOffset = (GVFrameStamp)(playTime - frameStamp);

	gviSource = gviFindSourceInList(sourceList, source);
	if(!gviSource)
	{
		gviSource = gviAddSourceToList(sourceList, source);

		if(!gviSource)
			return;

		gviSource->m_clockOffset = clockOffset;
		gviSource->m_clockOffsetAverage = (float)clockOffset;

		gviSource->m_finishedTalkingTime = (GVFrameStamp)(frameStamp + gviSource->m_clockOffset);
	}
	else
	{
		if((clockOffset < gviSource->m_clockOffsetAverage) && gviIsFrameStampGT(clockOffset, (GVFrameStamp)gviSource->m_clockOffsetAverage))
		{
			unwrappedClockOffset = (clockOffset + GVI_FRAMESTAMP_MAX);
		}
		else if((gviSource->m_clockOffsetAverage < clockOffset) && gviIsFrameStampGT((GVFrameStamp)gviSource->m_clockOffsetAverage, clockOffset))
		{
			unwrappedClockOffset = (clockOffset - GVI_FRAMESTAMP_MAX);
		}
		else
		{
			unwrappedClockOffset = clockOffset;
		}

		gviSource->m_clockOffsetAverage =
			(float)((gviSource->m_clockOffsetAverage * GVI_CLOCK_OFFSET_AVERAGING_FACTOR) +
			(unwrappedClockOffset * (1.0 - GVI_CLOCK_OFFSET_AVERAGING_FACTOR)));
		if(gviSource->m_clockOffsetAverage < 0)
			gviSource->m_clockOffsetAverage += GVI_FRAMESTAMP_MAX;
		else if(gviSource->m_clockOffsetAverage >= GVI_FRAMESTAMP_MAX)
			gviSource->m_clockOffsetAverage -= GVI_FRAMESTAMP_MAX;

		// calculate the divergence
		divergence = (GVFrameStamp)(gviSource->m_clockOffset - (GVFrameStamp)gviSource->m_clockOffsetAverage);

		if(gviIsFrameStampGT(divergence, GVI_DIVERGENCE_HIGH_WATERMARK))
		{
#if GVI_SHOW_SKEW_CORRECTIONS
			static int dropCount;
			printf("DROP: %d\n", ++dropCount);
#endif

			gviSource->m_clockOffset--;

			if(len == GVIEncodedFrameSize)
				return;

			packet += GVIEncodedFrameSize;
			len -= GVIEncodedFrameSize;
		}
		else if(gviIsFrameStampGT(GVI_DIVERGENCE_LOW_WATERMARK, divergence))
		{
#if GVI_SHOW_SKEW_CORRECTIONS
			static int insertCount;
			printf("INSERT: %d\n", ++insertCount);
#endif

			gviSource->m_clockOffset++;
		}

		if(gviIsFrameStampGT(currentPlayClock, (GVFrameStamp)(frameStamp + gviSource->m_clockOffset)))
			return;
	}

	gviAddPacketToSource(gviSource, frameStamp, packet, len, mute);
}

GVBool gviWriteSourcesToBuffer(GVISourceList sourceList, GVFrameStamp startTime,
                               GVSample * sampleBuffer, int numFrames)
{
	GVISource * source;
	GVFrameStamp timeSliceEnd;
	GVIPendingFrame * frame;
	int i;
	GVBool result = GVFalse;

	timeSliceEnd = (GVFrameStamp)(startTime + numFrames);

	memset(sampleBuffer, 0, (unsigned int)numFrames * GVIBytesPerFrame);

	for(i = 0 ; i < GVI_MAX_SOURCES ; i++)
	{
		source = &sourceList[i];

		if(!source->m_inUse)
			continue;

		while(source->m_frameHead.m_next)
		{
			frame = source->m_frameHead.m_next;

			if(gviIsFrameStampGTE(frame->m_frameStamp, timeSliceEnd))
				break;

			if(gviIsFrameStampGTE(frame->m_frameStamp, startTime))
			{
				GVSample * writePtr = (sampleBuffer + ((GVFrameStamp)(frame->m_frameStamp - startTime) * GVISamplesPerFrame));
#if GVI_PRE_DECODE
				int j;
				for(j = 0 ; j < GVISamplesPerFrame ; j++)
					writePtr[j] += frame->m_frame[j];
#else
				gviDecodeAdd(writePtr, frame->m_frame, source->m_decoderData);
#endif

				source->m_isTalking = GVTrue;

				result = GVTrue;
			}

			source->m_frameHead.m_next = frame->m_next;

			gviPutPendingFrame(frame);
		}

		if(!source->m_frameHead.m_next && gviIsFrameStampGTE(timeSliceEnd, source->m_finishedTalkingTime))
			gviFreeSource(source);
	}

	return result;

}
