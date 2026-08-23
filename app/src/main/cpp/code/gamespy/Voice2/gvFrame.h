
#ifndef _GV_FRAME_H_
#define _GV_FRAME_H_

#include "gvMain.h"

#define GVI_FRAMESTAMP_MAX   0xFFFF

#if defined(_MACOSX)
	#define GVI_PRE_DECODE 1
#else
	#define GVI_PRE_DECODE 0
#endif

typedef struct GVIPendingFrame
{
	GVFrameStamp m_frameStamp;
	struct GVIPendingFrame * m_next;
#if GVI_PRE_DECODE
	GVSample m_frame[1];
#else
	GVByte m_frame[1];
#endif
} GVIPendingFrame;

void gviFramesStartup(void);
void gviFramesCleanup(void);

GVIPendingFrame * gviGetPendingFrame(void);
void gviPutPendingFrame(GVIPendingFrame * frame);

GVBool gviIsFrameStampGT(GVFrameStamp a, GVFrameStamp b);
GVBool gviIsFrameStampGTE(GVFrameStamp a, GVFrameStamp b);

#endif
