
#ifndef _GV_UTIL_H_
#define _GV_UTIL_H_

#include "gvMain.h"
#include "gvCodec.h"

GVScalar gviGetSamplesVolume(const GVSample * samplesPtr, int numSamples);

GVBool gviIsOverThreshold(const GVSample * samplesPtr, int numSamples, GVScalar threshold);

int gviRoundUpToNearestMultiple(int value, int base);
int gviRoundDownToNearestMultiple(int value, int base);
int gviRoundToNearestMultiple(int value, int base);

int gviMultiplyByBytesPerMillisecond(int value);
int gviDivideByBytesPerMillisecond(int value);

#endif
