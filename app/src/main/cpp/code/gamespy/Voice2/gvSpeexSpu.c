#include "gvSpeexSpu.h"
#include <speex.h>
#include "gvCodec.h"

#define GVI_SPEEX_ENCODED_BUFFER 128 
struct SpursSpeexTaskOutput gSpeexTaskOutput;
char *gviSpeexEncoderStateBuffer;
static GVBool gviSpeexEncoderInitialized;
static int gviSpeexEncodedFrameSize;
static int gviSpeexSamplesPerFrame;
char *gviSpeexEncodedBuffer;
short *gviSpeexDecodedBuffer;
#if defined(GVI_NOT_USING_SPURS_DECODE_TASK)
static float * gviSpeexBuffer;
static SpeexBits gviSpeexBits;
#endif


GVBool gviSpeexInitialize(int quality, GVRate sampleRate)
{
	if(gviSpeexEncoderInitialized)
		return GVFalse;

	gviSpeexEncoderStateBuffer = (char *)gsimemalign(128,SPEEX_ENCODER_STATE_BUFFER_SIZE);
	gSpeexTaskOutput.mSpeexReturnCode = -1;

#if defined(GVI_NOT_USING_SPURS_DECODE_TASK)
	speex_bits_init(&gviSpeexBits);
#endif 

	if (initializeSpursSampleTask() != 0)
		return GVFalse;
	
	if (issueSampleTaskEncodeInit(quality, sampleRate, &gSpeexTaskOutput,gviSpeexEncoderStateBuffer,SPEEX_ENCODER_STATE_BUFFER_SIZE) != 0)
		return GVFalse;
	
	assert(gSpeexTaskOutput.mSpeexReturnCode == 0);
	if (gSpeexTaskOutput.mSpeexInitialized != GVTrue)
	{
		return GVFalse;
	}

	gviSpeexSamplesPerFrame = gSpeexTaskOutput.mSpeexSamplesPerFrame;
	gviSpeexEncodedFrameSize = gSpeexTaskOutput.mSpeexEncodedFrameSize;

#if defined(GVI_NOT_USING_SPURS_DECODE_TASK)
	gviSpeexBuffer = (float *)gsimalloc(gviSpeexSamplesPerFrame * sizeof(float));
#endif
	gviSpeexEncodedBuffer = (char *)gsimemalign(128, GVI_SPEEX_ENCODED_BUFFER);
	gviSpeexDecodedBuffer = (short *)gsimemalign(128, gviSpeexSamplesPerFrame*sizeof(short));
	gviSpeexEncoderInitialized = GVTrue;
	
	return GVTrue;
}

void gviSpeexCleanup(void)
{
	if(!gviSpeexEncoderInitialized)
		return;
	
#ifdef GVI_NOT_USING_SPURS_DECODE_TASK
	gsifree(gviSpeexBuffer);

	speex_bits_destroy(&gviSpeexBits);
#endif
		
	gsifree(gviSpeexEncoderStateBuffer);
	gsifree(gviSpeexEncodedBuffer);
	gsifree(gviSpeexDecodedBuffer);
	shutdownSpursTask();

	gviSpeexEncoderInitialized = GVFalse;
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
#ifdef GVI_NOT_USING_SPURS_DECODE_TASK
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

#else 
	char *decoder = (char *)gsimemalign(128, SPEEX_DECODER_STATE_BUFFER_SIZE);
	gSpeexTaskOutput.mSpeexReturnCode = -1;
	
	if (issueSampleTaskDecodeInit(decoder, SPEEX_DECODER_STATE_BUFFER_SIZE, gviGetSampleRate(), &gSpeexTaskOutput) != 0)
		return GVFalse;
	
	if (gSpeexTaskOutput.mSpeexReturnCode != 0)
		return GVFalse;

	*data = decoder;
	return GVTrue;
#endif 
}

void gviSpeexFreeDecoder(GVDecoderData data)
{
#ifdef GVI_NOT_USING_SPURS_DECODE_TASK
	speex_decoder_destroy((void *)data);
#else
	gsifree(data);
#endif
}


void gviSpeexEncode(GVByte * out, const GVSample * in)
{	
	int immediateReturn = 0;
	gSpeexTaskOutput.mSpeexInitialized = 1;
	gSpeexTaskOutput.mSpeexReturnCode = -1;
	memset(gviSpeexEncodedBuffer, 0, GVI_SPEEX_ENCODED_BUFFER);
	immediateReturn = issueSampleTaskEncode((short *)in, gviSpeexSamplesPerFrame, gviSpeexEncodedFrameSize, (char *)gviSpeexEncodedBuffer, 
		GVI_SPEEX_ENCODED_BUFFER, &gSpeexTaskOutput,gviSpeexEncoderStateBuffer,SPEEX_ENCODER_STATE_BUFFER_SIZE);
	assert(immediateReturn == 0);
	assert(gSpeexTaskOutput.mSpeexReturnCode == 0);
	memcpy(out, gviSpeexEncodedBuffer, gviSpeexEncodedFrameSize);
	assert(gSpeexTaskOutput.mSpeexOutBufferSize == gviSpeexEncodedFrameSize);	
}

void gviSpeexDecodeAdd(GVSample * out, const GVByte * in, GVDecoderData data)
{
#ifdef GVI_NOT_USING_SPURS_DECODE_TASK
	int rcode;
	int i;

	speex_bits_read_from(&gviSpeexBits, (char *)in, gviSpeexEncodedFrameSize);

	rcode = speex_decode((void *)data, &gviSpeexBits, gviSpeexBuffer);
	assert(rcode == 0);

	for(i = 0 ; i < gviSpeexSamplesPerFrame ; i++)
		out[i] = out[i] + (GVSample)gviSpeexBuffer[i];
#else
	int immediateReturn = 0, i;
	gSpeexTaskOutput.mSpeexInitialized = 1;
	gSpeexTaskOutput.mSpeexReturnCode = -1;
	memset(gviSpeexEncodedBuffer, 0, GVI_SPEEX_ENCODED_BUFFER);
	memcpy(gviSpeexEncodedBuffer, in, gviSpeexEncodedFrameSize);
	immediateReturn = issueSampleTaskDecodeAdd(data, SPEEX_DECODER_STATE_BUFFER_SIZE, gviSpeexEncodedBuffer, GVI_SPEEX_ENCODED_BUFFER, 
		gviSpeexEncodedFrameSize, gviSpeexDecodedBuffer, gviSpeexSamplesPerFrame, &gSpeexTaskOutput);
	for (i = 0; i < gviSpeexSamplesPerFrame; i++)
		out[i] = out[i] + (GVSample)gviSpeexDecodedBuffer[i];
	assert(immediateReturn == 0);
	assert(gSpeexTaskOutput.mSpeexReturnCode == 0);
#endif
}

void gviSpeexDecodeSet(GVSample * out, const GVByte * in, GVDecoderData data)
{
#ifdef GVI_USE_SPURS_DECODE_TASK
	int rcode;
	int i;

	speex_bits_read_from(&gviSpeexBits, (char *)in, gviSpeexEncodedFrameSize);

	rcode = speex_decode((void *)data, &gviSpeexBits, gviSpeexBuffer);

	assert(rcode == 0);

	for(i = 0 ; i < gviSpeexSamplesPerFrame ; i++)
		out[i] = (GVSample)gviSpeexBuffer[i];
#else
	int immediateReturn = 0;
	gSpeexTaskOutput.mSpeexInitialized = 1;
	gSpeexTaskOutput.mSpeexReturnCode = -1;
	memset(gviSpeexEncodedBuffer, 0, GVI_SPEEX_ENCODED_BUFFER);
	memcpy(gviSpeexEncodedBuffer, in, gviSpeexEncodedFrameSize);
	immediateReturn = issueSampleTaskDecodeSet(data, SPEEX_DECODER_STATE_BUFFER_SIZE, gviSpeexEncodedBuffer, GVI_SPEEX_ENCODED_BUFFER, 
		gviSpeexEncodedFrameSize, out, gviSpeexSamplesPerFrame, &gSpeexTaskOutput);
	assert(immediateReturn == 0);
	assert(gSpeexTaskOutput.mSpeexReturnCode == 0);
#endif
}

void gviSpeexResetEncoder(void)
{
	speex_encoder_ctl((void *)gviSpeexEncoderStateBuffer, SPEEX_RESET_STATE, NULL);
}
