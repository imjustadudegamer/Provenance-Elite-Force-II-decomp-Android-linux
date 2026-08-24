/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#include "gvCodec.h"
#include "gvFrame.h"

#if !defined(GV_NO_DEFAULT_CODEC)
	#if defined(_PS2)
		#include "gvLogitechPS2Codecs.h"
	#elif defined(_PSP)
		#include "gvGSM.h"
	#else
		#include "gvSpeex.h"
	#endif
#endif

#define GVI_RAW_BASE_SAMPLES_PER_FRAME 160

int GVISamplesPerFrame;
int GVIBytesPerFrame;
int GVIEncodedFrameSize;
int GVISampleRate;
int GVIBytesPerSecond;
static GVCustomCodecInfo GVICodecInfo;
#if !defined(GV_NO_DEFAULT_CODEC)
static GVBool GVICleanupInternalCodec;
#endif
#ifndef _PSP
static GVBool GVIRawCodec;
#endif

#if !defined(GV_NO_DEFAULT_CODEC)
static void gviCleanupCodec(void)
{
#if defined(_PS2)
	gviLGCodecCleanup();
#elif defined(_PSP)
	gviGSMCleanup();
#else
	gviSpeexCleanup();
#endif
}
#endif

void gviCodecsInitialize(void)
{
#if defined(_PSP)
	gviSetSampleRate(GVRate_11KHz);
#else
	gviSetSampleRate(GVRate_8KHz);
#endif

#if !defined(GV_NO_DEFAULT_CODEC)
	GVICleanupInternalCodec = GVFalse;
#endif
}

void gviCodecsCleanup(void)
{
#if !defined(GV_NO_DEFAULT_CODEC)
	if(GVICleanupInternalCodec)
	{
		gviCleanupCodec();
		GVICleanupInternalCodec = GVFalse;
	}
#endif
}

#if !defined(GV_NO_DEFAULT_CODEC)

#if defined(_PS2)
static GVBool gviSetInternalCodec(GVCodec codec)
{
	GVCustomCodecInfo info;
	const char * name;

	if(codec == GVCodecSuperHighQuality)
		name = "uLaw";
	else if(codec == GVCodecHighQuality)
		name = "G723.24";
	else if(codec == GVCodecAverage)
		name = "GSM";
	else if(codec == GVCodecLowBandwidth)
		name = "SPEEX";
	else if(codec == GVCodecSuperLowBandwidth)
		name = "LPC10";
	else
		return GVFalse;

	if(!gviLGCodecInitialize(name))
		return GVFalse;

	info.m_samplesPerFrame = gviLGCodecGetSamplesPerFrame();
	info.m_encodedFrameSize = gviLGCodecGetEncodedFrameSize();
	info.m_newDecoderCallback = NULL;
	info.m_freeDecoderCallback = NULL;
	info.m_encodeCallback = gviLGCodecEncode;
	info.m_decodeAddCallback = gviLGCodecDecodeAdd;
	info.m_decodeSetCallback = gviLGCodecDecodeSet;

	gviSetCustomCodec(&info);

	return GVTrue;
}
#elif defined(_PSP)
static GVBool gviSetInternalCodec(GVCodec codec)
{
	GVCustomCodecInfo info;

	if(!gviGSMInitialize())
		return GVFalse;

	info.m_samplesPerFrame = gviGSMGetSamplesPerFrame();
	info.m_encodedFrameSize = gviGSMGetEncodedFrameSize();
	info.m_newDecoderCallback = gviGSMNewDecoder;
	info.m_freeDecoderCallback = gviGSMFreeDecoder;
	info.m_encodeCallback = gviGSMEncode;
	info.m_decodeAddCallback = gviGSMDecodeAdd;
	info.m_decodeSetCallback = gviGSMDecodeSet;

	gviSetCustomCodec(&info);

	GSI_UNUSED(codec);

	return GVTrue;
}
#else
static GVBool gviSetInternalCodec(GVCodec codec)
{
	GVCustomCodecInfo info;
	int quality;

	if(codec == GVCodecSuperHighQuality)
		quality = 10;
	else if(codec == GVCodecHighQuality)
		quality = 7;
	else if(codec == GVCodecAverage)
		quality = 4;
	else if(codec == GVCodecLowBandwidth)
		quality = 2;
	else if(codec == GVCodecSuperLowBandwidth)
		quality = 1;
	else
		return GVFalse;

	if(!gviSpeexInitialize(quality, GVISampleRate))
		return GVFalse;

	info.m_samplesPerFrame = gviSpeexGetSamplesPerFrame();
	info.m_encodedFrameSize = gviSpeexGetEncodedFrameSize();
	info.m_newDecoderCallback = gviSpeexNewDecoder;
	info.m_freeDecoderCallback = gviSpeexFreeDecoder;
	info.m_encodeCallback = gviSpeexEncode;
	info.m_decodeAddCallback = gviSpeexDecodeAdd;
	info.m_decodeSetCallback = gviSpeexDecodeSet;

	gviSetCustomCodec(&info);

	return GVTrue;
}
#endif

#endif

static void gviRawEncode(GVByte * out, const GVSample * in)
{
	GVSample * sampleOut = (GVSample *)out;
	int i;

	for(i = 0 ; i < GVISamplesPerFrame; i++)
		sampleOut[i] = htons(in[i]);
}

static void gviRawDecodeAdd(GVSample * out, const GVByte * in, GVDecoderData data)
{
	const GVSample * sampleIn = (const GVSample *)in;
	int i;

	for(i = 0 ; i < GVISamplesPerFrame; i++)
		out[i] = out[i] + ntohs(sampleIn[i]);

	GSI_UNUSED(data);
}

static void gviRawDecodeSet(GVSample * out, const GVByte * in, GVDecoderData data)
{
	const GVSample * sampleIn = (const GVSample *)in;
	int i;

	for(i = 0 ; i < GVISamplesPerFrame; i++)
		out[i] = ntohs(sampleIn[i]);

	GSI_UNUSED(data);
}

static void gviSetRawCodec(void)
{
	GVCustomCodecInfo info;
	memset(&info, 0, sizeof(info));
	if (GVISampleRate == GVRate_8KHz)
		info.m_samplesPerFrame = GVI_RAW_BASE_SAMPLES_PER_FRAME;
	else if (GVISampleRate == GVRate_16KHz)
		info.m_samplesPerFrame = GVI_RAW_BASE_SAMPLES_PER_FRAME * 2;

	info.m_encodedFrameSize = (info.m_samplesPerFrame * GV_BYTES_PER_SAMPLE);
	info.m_newDecoderCallback = NULL;
	info.m_freeDecoderCallback = NULL;
	info.m_encodeCallback = gviRawEncode;
	info.m_decodeAddCallback = gviRawDecodeAdd;
	info.m_decodeSetCallback = gviRawDecodeSet;

	gviSetCustomCodec(&info);
}

GVBool gviSetCodec(GVCodec codec)
{
#if !defined(GV_NO_DEFAULT_CODEC)
	if(GVICleanupInternalCodec)
	{
		gviCleanupCodec();
		GVICleanupInternalCodec = GVFalse;
	}
#endif

	if(codec == GVCodecRaw)
	{
		gviSetRawCodec();
		#ifndef _PSP
		GVIRawCodec = GVTrue;
		#endif
		return GVTrue;
	}
	else
		#ifndef _PSP
		GVIRawCodec = GVFalse;
		#endif

#if !defined(GV_NO_DEFAULT_CODEC)
	if(!gviSetInternalCodec(codec))
		return GVFalse;

	GVICleanupInternalCodec = GVTrue;

	return GVTrue;
#else
	return GVFalse;
#endif
}

void gviSetCustomCodec(GVCustomCodecInfo * info)
{
	memcpy(&GVICodecInfo, info, sizeof(GVCustomCodecInfo));
	GVISamplesPerFrame = info->m_samplesPerFrame;
	GVIEncodedFrameSize = info->m_encodedFrameSize;

	GVIBytesPerFrame = (GVISamplesPerFrame * (int)GV_BYTES_PER_SAMPLE);

	gviFramesStartup();
}

void gviSetSampleRate(GVRate sampleRate)
{
	GVISampleRate     = sampleRate;
	GVIBytesPerSecond = GVISampleRate * GV_BYTES_PER_SAMPLE;
}

GVRate gviGetSampleRate(void)
{
	return GVISampleRate;
}


GVBool gviNewDecoder(GVDecoderData * data)
{
	if(!GVICodecInfo.m_newDecoderCallback)
	{
		*data = NULL;
		return GVTrue;
	}

	return GVICodecInfo.m_newDecoderCallback(data);
}

void gviFreeDecoder(GVDecoderData data)
{
	if(GVICodecInfo.m_freeDecoderCallback)
		GVICodecInfo.m_freeDecoderCallback(data);
}

void gviEncode(GVByte * out, const GVSample * in)
{
	GVICodecInfo.m_encodeCallback(out, in);
}

void gviDecodeAdd(GVSample * out, const GVByte * in, GVDecoderData data)
{
	GVICodecInfo.m_decodeAddCallback(out, in, data);
}

void gviDecodeSet(GVSample * out, const GVByte * in, GVDecoderData data)
{
	if(GVICodecInfo.m_decodeSetCallback)
	{
		GVICodecInfo.m_decodeSetCallback(out, in, data);
	}
	else
	{
		memset(out, 0, GVIBytesPerFrame);
		GVICodecInfo.m_decodeAddCallback(out, in, data);
	}
}

void gviResetEncoder(void)
{
#if !defined(GV_NO_DEFAULT_CODEC) && !defined(_PS2) && !defined(_PSP)

	if (!GVIRawCodec)
		gviSpeexResetEncoder();
#endif
}
