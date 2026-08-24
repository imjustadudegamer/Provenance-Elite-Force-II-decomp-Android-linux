#include "gvGSM.h"
#include <gsm.h>

#define GVI_SAMPLES_PER_FRAME  160
#define GVI_ENCODED_FRAME_SIZE  33

static GVBool gviGSMInitialized;
static gsm gviGSMEncoderState;

GVBool gviGSMInitialize(void)
{
	const int gsm_ltp = 1;

	if(gviGSMInitialized)
		return GVFalse;

	gviGSMEncoderState = gsm_create();
	if(!gviGSMEncoderState)
		return GVFalse;

	gsm_option(gviGSMEncoderState, GSM_OPT_LTP_CUT, &gsm_ltp);

	gviGSMInitialized = GVTrue;

	return GVTrue;
}

void gviGSMCleanup(void)
{
	if(!gviGSMInitialized)
		return;

	gsm_destroy(gviGSMEncoderState);
	gviGSMEncoderState = NULL;

	gviGSMInitialized = GVFalse;
}

int gviGSMGetSamplesPerFrame(void)
{
	return GVI_SAMPLES_PER_FRAME;
}

int gviGSMGetEncodedFrameSize(void)
{
	return GVI_ENCODED_FRAME_SIZE;
}

GVBool gviGSMNewDecoder(GVDecoderData * data)
{
	gsm decoder;

	decoder = gsm_create();
	if(!decoder)
		return GVFalse;

	*data = decoder;
	return GVTrue;
}

void gviGSMFreeDecoder(GVDecoderData data)
{
	gsm_destroy((gsm)data);
}

void gviGSMEncode(GVByte * out, const GVSample * in)
{
	gsm_encode((gsm)gviGSMEncoderState, (gsm_signal*)in, (gsm_byte*)out);
}

void gviGSMDecodeAdd(GVSample * out, const GVByte * in, GVDecoderData data)
{
	GVSample temp[GVI_SAMPLES_PER_FRAME];
	int i;

	gsm_decode((gsm)data, (gsm_byte*)in, (gsm_signal*)temp);

	for(i = 0 ; i < GVI_SAMPLES_PER_FRAME ; i++)
		out[i] += temp[i];
}

void gviGSMDecodeSet(GVSample * out, const GVByte * in, GVDecoderData data)
{
	gsm_decode((gsm)data, (gsm_byte*)in, (gsm_signal*)out);
}
