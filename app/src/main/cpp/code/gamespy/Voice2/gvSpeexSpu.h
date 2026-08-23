
#ifndef _GV_SPEEXSPU_H_
#define _GV_SPEEXSPU_H_

#include "../common/gsPlatform.h"
#include "gvMain.h"
#include "SpuSpeexTaskOutput.h"
#include "SpursSpeexCInterface.h"


GVBool gviSpeexInitialize(int quality, GVRate sampleRate);
void gviSpeexCleanup(void);

int gviSpeexGetSamplesPerFrame(void);
int gviSpeexGetEncodedFrameSize(void);

GVBool gviSpeexNewDecoder(GVDecoderData * data);
void gviSpeexFreeDecoder(GVDecoderData data);

void gviSpeexEncode(GVByte * out, const GVSample * in);
void gviSpeexDecodeAdd(GVSample * out, const GVByte * in, GVDecoderData data);
void gviSpeexDecodeSet(GVSample * out, const GVByte * in, GVDecoderData data);

void gviSpeexResetEncoder(void);

#endif
