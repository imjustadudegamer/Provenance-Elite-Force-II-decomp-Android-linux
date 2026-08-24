#include "gvSpeex.h"
#include <speex.h>
#include "gvCodec.h"


static GVBool gviSpeexInitialized;
static void * gviSpeexEncoderState;
static SpeexBits gviSpeexBits;
static int gviSpeexEncodedFrameSize;
static int gviSpeexSamplesPerFrame;

static float * gviSpeexBuffer;

GVBool gviSpeexInitialize(int quality, GVRate sampleRate)
{
	int rate;
	int bitsPerFrame;
	int samplesPerSecond;
	
	if(gviSpeexInitialized)
		return GVFalse;

	if (sampleRate == GVRate_8KHz)
		gviSpeexEncoderState = speex_encoder_init(&speex_nb_mode);
	else if (sampleRate == GVRate_16KHz)
		gviSpeexEncoderState = speex_encoder_init(&speex_wb_mode);
	else
		return GVFalse;

	if(!gviSpeexEncoderState)
		return GVFalse;

	samplesPerSecond = sampleRate;
	speex_encoder_ctl(gviSpeexEncoderState, SPEEX_SET_SAMPLING_RATE, &samplesPerSecond);

	speex_encoder_ctl(gviSpeexEncoderState, SPEEX_GET_FRAME_SIZE, &gviSpeexSamplesPerFrame);

	speex_encoder_ctl(gviSpeexEncoderState, SPEEX_SET_QUALITY, &quality);

	speex_bits_init(&gviSpeexBits);

	speex_encoder_ctl(gviSpeexEncoderState, SPEEX_GET_BITRATE, &rate);

	bitsPerFrame = (rate / (sampleRate / gviSpeexSamplesPerFrame));

	gviSpeexEncodedFrameSize = (bitsPerFrame / 8);
	if (bitsPerFrame % 8)
		gviSpeexEncodedFrameSize++;

	gviSpeexBuffer = (float *)gsimalloc(gviSpeexSamplesPerFrame * sizeof(float));

	gviSpeexInitialized = GVTrue;

	return GVTrue;
}

void gviSpeexCleanup(void)
{
	if(!gviSpeexInitialized)
		return;

	gsifree(gviSpeexBuffer);

	speex_encoder_destroy(gviSpeexEncoderState);
	gviSpeexEncoderState = NULL;

	speex_bits_destroy(&gviSpeexBits);

	gviSpeexInitialized = GVFalse;
}

int gviSpeexGetSamplesPerFrame(void)
{
	return gviSpeexSamplesPerFrame;
}

int gviSpeexGetEncodedFrameSize(void)
{
	return gviSpeexEncodedFrameSize;
}

GVBool gviSpeexNewDecoder(GVDecoderData * data)
{
	void * decoder;
	int perceptualEnhancement = 1;
	
	if (gviGetSampleRate() == GVRate_8KHz)
		decoder = speex_decoder_init(&speex_nb_mode);
	else if (gviGetSampleRate() == GVRate_16KHz)
		decoder = speex_decoder_init(&speex_wb_mode);
	else
		return GVFalse;

	if(!decoder)
		return GVFalse;

	speex_decoder_ctl(decoder, SPEEX_SET_ENH, &perceptualEnhancement);

	*data = decoder;
	return GVTrue;
}

void gviSpeexFreeDecoder(GVDecoderData data)
{
	speex_decoder_destroy((void *)data);
}

void gviSpeexEncode(GVByte * out, const GVSample * in)
{
	int bytesWritten;
	int i;

	for(i = 0 ; i < gviSpeexSamplesPerFrame ; i++)
		gviSpeexBuffer[i] = in[i];

	speex_bits_reset(&gviSpeexBits);

	speex_encode(gviSpeexEncoderState, gviSpeexBuffer, &gviSpeexBits);

	bytesWritten = speex_bits_write(&gviSpeexBits, (char *)out, gviSpeexEncodedFrameSize);
	assert(bytesWritten == gviSpeexEncodedFrameSize);
}

void gviSpeexDecodeAdd(GVSample * out, const GVByte * in, GVDecoderData data)
{
	int rcode;
	int i;

	speex_bits_read_from(&gviSpeexBits, (char *)in, gviSpeexEncodedFrameSize);

	rcode = speex_decode((void *)data, &gviSpeexBits, gviSpeexBuffer);
	assert(rcode == 0);

	for(i = 0 ; i < gviSpeexSamplesPerFrame ; i++)
		out[i] = out[i] + (GVSample)gviSpeexBuffer[i];
}

void gviSpeexDecodeSet(GVSample * out, const GVByte * in, GVDecoderData data)
{
	int rcode;
	int i;

	speex_bits_read_from(&gviSpeexBits, (char *)in, gviSpeexEncodedFrameSize);

	rcode = speex_decode((void *)data, &gviSpeexBits, gviSpeexBuffer);
	assert(rcode == 0);

	for(i = 0 ; i < gviSpeexSamplesPerFrame ; i++)
		out[i] = (GVSample)gviSpeexBuffer[i];
}

void gviSpeexResetEncoder(void)
{
	speex_encoder_ctl(gviSpeexEncoderState, SPEEX_RESET_STATE, NULL);
}
