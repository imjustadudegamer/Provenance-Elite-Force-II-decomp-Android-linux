
#ifndef _GV_GSM_H_
#define _GV_GSM_H_

#include "gvMain.h"


GVBool gviGSMInitialize(void);
void gviGSMCleanup(void);

int gviGSMGetSamplesPerFrame(void);
int gviGSMGetEncodedFrameSize(void);

GVBool gviGSMNewDecoder(GVDecoderData * data);
void gviGSMFreeDecoder(GVDecoderData data);

void gviGSMEncode(GVByte * out, const GVSample * in);
void gviGSMDecodeAdd(GVSample * out, const GVByte * in, GVDecoderData data);
void gviGSMDecodeSet(GVSample * out, const GVByte * in, GVDecoderData data);

#endif
