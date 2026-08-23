#include "gvFrame.h"
#include "gvCodec.h"

#define GVI_INCOMING_PACKET_TIMEFRAME_FRAMES      (GVI_FRAMESTAMP_MAX / 2)

#define GVI_PREALLOCATED_FRAMES                   200

#if defined(_PS2) || defined(_PSP)
#define GVI_DYNAMICALLY_ALLOCATE_FRAMES           0
#else
#define GVI_DYNAMICALLY_ALLOCATE_FRAMES           1
#endif

static GVIPendingFrame * GVIAvailableFrames;

GVBool gviIsFrameStampGT(GVFrameStamp a, GVFrameStamp b)
{
	return ((GVFrameStamp)(b - a) > GVI_INCOMING_PACKET_TIMEFRAME_FRAMES);
}

GVBool gviIsFrameStampGTE(GVFrameStamp a, GVFrameStamp b)
{
	return ((GVFrameStamp)(b - a - 1) > GVI_INCOMING_PACKET_TIMEFRAME_FRAMES);
}

static void gviFreePendingFrame(GVIPendingFrame * frame)
{
	gsifree(frame);
}

static GVIPendingFrame * gviNewPendingFrame(void)
{
	GVIPendingFrame * frame;

#if GVI_PRE_DECODE
	frame = (GVIPendingFrame *)gsimalloc(sizeof(GVIPendingFrame) + GVIBytesPerFrame - sizeof(GVSample));
#else
	frame = (GVIPendingFrame *)gsimalloc(sizeof(GVIPendingFrame) + GVIEncodedFrameSize - 1);
#endif

	return frame;
}

void gviPutPendingFrame(GVIPendingFrame * frame)
{
	frame->m_next = GVIAvailableFrames;
	GVIAvailableFrames = frame;
}

GVIPendingFrame * gviGetPendingFrame(void)
{
	GVIPendingFrame * frame;

	if(GVIAvailableFrames)
	{
		frame = GVIAvailableFrames;
		GVIAvailableFrames = frame->m_next;
		return frame;
	}

#if GVI_DYNAMICALLY_ALLOCATE_FRAMES
	return gviNewPendingFrame();
#else
	return NULL;
#endif
}

void gviFramesStartup(void)
{
	GVIPendingFrame * frame;
	int i;

	if(GVIAvailableFrames)
		gviFramesCleanup();

	GVIAvailableFrames = NULL;

	for(i = 0 ; i < GVI_PREALLOCATED_FRAMES ; i++)
	{
		frame = gviNewPendingFrame();
		if(!frame)
			return;
		frame->m_next = GVIAvailableFrames;
		GVIAvailableFrames = frame;
	}
}

void gviFramesCleanup(void)
{
	GVIPendingFrame * next;

	while(GVIAvailableFrames)
	{
		next = GVIAvailableFrames->m_next;
		gviFreePendingFrame(GVIAvailableFrames);
		GVIAvailableFrames = next;
	}
}
