// NOTE:  this code has been provided by Sony for usage in Speex SPURS Manager


#ifndef SPU_SAMPLE_TASK_PROCESS_H
#define SPU_SAMPLE_TASK_PROCESS_H

#include <assert.h>


#include "spursPlatformDefinitions.h"

#include <stdlib.h>

#include "spursAlignedObjectArray.h"

#include "SpuSpeexTaskOutput.h"

struct SpursSpeexTaskDesc
{
	SpursSpeexTaskDesc()
		:mDebugPause(false)
	{
		
	}
	int           mQuality; 
	int           mSamplesPerSecond;
	int           mEncodedFrameSize;
	void          *mInputBuffer;     
	unsigned int   mInputBufferSize; 

	SpursSpeexTaskOutput	*mSpeexTaskOutput;
	void            *mOutputBuffer; 
	unsigned int     mOutputBufferSize;

	char            *mSpeexStateBuffer; 
	unsigned int     mSpeexStateBufferSize;
	bool	mDebugPause;

	uint16_t 	  mTaskId;
} POST_ALIGN(128);

#define SPEEX_TASK_ENCODE_COMMAND      10
#define SPEEX_TASK_ENCODE_INIT_COMMAND 11
#define SPEEX_TASK_DECODEADD_COMMAND   12
#define SPEEX_TASK_DECODESET_COMMAND   13
#define SPEEX_TASK_DECODE_INIT_COMMAND 14



class SpursSpeexTaskManager
{
	spursAlignedObjectArray<bool>	m_taskBusy;
	spursAlignedObjectArray<SpursSpeexTaskDesc>mSpursSpeexTaskDesc;
	
	unsigned int   m_numBusyTasks;

	unsigned int   m_currentTask;

	bool m_initialized;

	
	class	spursThreadSupportInterface*	m_threadInterface;

	unsigned int	m_maxNumOutstandingTasks;


	int issueTask(SpursSpeexTaskDesc& taskDesc,uint32_t uiCommand);


public:
	SpursSpeexTaskManager(spursThreadSupportInterface*	threadInterface, unsigned int maxNumOutstandingTasks);
	
	~SpursSpeexTaskManager();
	
	int initialize();

	int issueEncodeTask(int16_t * inBuffer, int inBufferSize, int encodedFrameSize, char *outBuffer, int outBufferSize, 
		                SpursSpeexTaskOutput *taskOuput,char *m_userAllocatedSpeexBuffer,int userAllocatedSpeexBufferSize);

	int issueEncodeInitTask(int theQuality,int theGviSpeexSamplesPerSecond,  SpursSpeexTaskOutput *taskOutput, char *m_userAllocatedSpeexBuffer,
	                        int userAllocatedSpeexBufferSize);

	int issueDecodeAddTask(char *decoderStateBuffer, int decoderStateBufferSize, char *inBuffer, int inBufferSize, int encodedFrameSize,
		                   short* outBuffer, int outBufferSize, struct SpursSpeexTaskOutput *taskOutput);

	int issueDecodeSetTask(char *decoderStateBuffer, int decoderStateBufferSize, char *inBuffer, int inBufferSize, int encodedFrameSize,
		                   short* outBuffer, int outBufferSize, struct SpursSpeexTaskOutput *taskOutput);
	
	int issueDecodeInitTask(char *decoderStateBuffer, int decoderStateBufferSize, int sampleRate, struct SpursSpeexTaskOutput *taskOutput);

	int flush();
};


#endif 

