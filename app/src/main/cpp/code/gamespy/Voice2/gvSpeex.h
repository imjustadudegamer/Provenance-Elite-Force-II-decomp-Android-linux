/*
GameSpy Voice2 SDK
Dan "Mr. Pants" Schoenblum
dan@gamespy.com

Copyright 2004 GameSpy Industries, Inc

devsupport@gamespy.com
http://gamespy.net
*/

#ifndef _GV_SPEEX_H_
#define _GV_SPEEX_H_

#include "gvMain.h"


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
