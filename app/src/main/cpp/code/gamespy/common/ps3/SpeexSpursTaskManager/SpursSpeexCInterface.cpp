#include "spursConfiguration.h"
#include "SpursSpeexCInterface.h"
#include <stdio.h>
#include "SpursSpeexTaskManager.h"
#include "spursSupportInterface.h"
#include <sys/spu_initialize.h> 

SpursSpeexTaskManager* gSpursSpeexTaskManager = 0;
SpursSupportInterface* gSpursSupport = 0;
const unsigned int MAX_SPURS_SPEEX_TASKS=1;


int initializeSpursSampleTask()
{
	gSpursSupport = new SpursSupportInterface();
	
	gSpursSpeexTaskManager = new SpursSpeexTaskManager(gSpursSupport,MAX_SPURS_SPEEX_TASKS);
	return gSpursSpeexTaskManager->initialize();	
}

int issueSampleTaskEncodeInit(int quality, int samplesPerFrame, SpursSpeexTaskOutput *taskOutput,  char *userAllocatedSpeexBuffer, int userAllocatedSpeexBufferSize)
{
	btAssert(gSpursSpeexTaskManager!=0);
	btAssert(gSpursSupport!=0);

	return gSpursSpeexTaskManager->issueEncodeInitTask(quality, samplesPerFrame, taskOutput,userAllocatedSpeexBuffer,userAllocatedSpeexBufferSize);

}

int issueSampleTaskEncode(short* inBuffer, int inBufferSize, int encodedFrameSize,  char *outBuffer, int outBufferSize, 
                          struct SpursSpeexTaskOutput *taskOuput,   char *userAllocatedSpeexBuffer, int userAllocatedSpeexBufferSize )
{
	btAssert(gSpursSpeexTaskManager!=0);
	btAssert(gSpursSupport!=0);

	return gSpursSpeexTaskManager->issueEncodeTask(inBuffer, inBufferSize, encodedFrameSize, outBuffer, outBufferSize, taskOuput,
		userAllocatedSpeexBuffer,userAllocatedSpeexBufferSize);

}

int issueSampleTaskDecodeAdd(char *decoderStateBuffer, int decoderStateBufferSize, char *inBuffer, int inBufferSize, int encodedFrameSize,
								 short* outBuffer, int outBufferSize, struct SpursSpeexTaskOutput *taskOutput)
{
	btAssert(gSpursSpeexTaskManager!=0);
	btAssert(gSpursSupport!=0);

	return gSpursSpeexTaskManager->issueDecodeAddTask(decoderStateBuffer, decoderStateBufferSize, inBuffer, inBufferSize, encodedFrameSize,
		outBuffer, outBufferSize, taskOutput);

}

int issueSampleTaskDecodeSet(char *decoderStateBuffer, int decoderStateBufferSize, char *inBuffer, int inBufferSize, int encodedFrameSize,
								 short* outBuffer, int outBufferSize, struct SpursSpeexTaskOutput *taskOutput)
{
	btAssert(gSpursSpeexTaskManager!=0);
	btAssert(gSpursSupport!=0);

	return gSpursSpeexTaskManager->issueDecodeSetTask(decoderStateBuffer, decoderStateBufferSize, inBuffer, inBufferSize, encodedFrameSize,
		outBuffer, outBufferSize, taskOutput);

}

int issueSampleTaskDecodeInit(char *decoderStateBuffer, int decoderStateBufferSize, int sampleRate, struct SpursSpeexTaskOutput *taskOutput)
{
	btAssert(gSpursSpeexTaskManager!=0);
	btAssert(gSpursSupport!=0);

	return gSpursSpeexTaskManager->issueDecodeInitTask(decoderStateBuffer, decoderStateBufferSize, sampleRate, taskOutput);

}



int shutdownSpursTask()
{
	btAssert(gSpursSpeexTaskManager!=0);
	btAssert(gSpursSupport!=0);
	delete gSpursSpeexTaskManager;
	delete gSpursSupport;
	if (spursConfiguration_terminate() != 0)
		return -1;
	return 0;
}
