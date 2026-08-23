

#define USE_SAMPLE_PROCESS 1
#ifdef USE_SAMPLE_PROCESS


#include "spursThreadSupportInterface.h"

#include <string.h>


#include "SpursSpeexTaskManager.h"


#include <stdio.h>


void	SampleThreadFunc(void* userPtr,void* lsMemory)
{
	printf("hello world\n");
}

void*	SamplelsMemoryFunc()
{
	return 0;
}



extern "C" 
{
	extern char SPU_SAMPLE_ELF_SYMBOL[];
};





SpursSpeexTaskManager::SpursSpeexTaskManager(spursThreadSupportInterface*	threadInterface, unsigned int maxNumOutstandingTasks)
:m_threadInterface(threadInterface),
m_maxNumOutstandingTasks(maxNumOutstandingTasks)
{

	m_taskBusy.resize(m_maxNumOutstandingTasks);
	mSpursSpeexTaskDesc.resize(m_maxNumOutstandingTasks);

	for (int i = 0; (unsigned int)i < m_maxNumOutstandingTasks; i++)
	{
		m_taskBusy[i] = false;
	}
	m_numBusyTasks = 0;
	m_currentTask = 0;

	m_initialized = false;
}

SpursSpeexTaskManager::~SpursSpeexTaskManager()
{
	m_threadInterface->stopSPU();
}



int SpursSpeexTaskManager::initialize()
{
#ifdef DEBUG_SPU_TASK_SCHEDULING
	printf("SpuSampleTaskProcess::initialize()\n");
#endif 
	
	for (int i = 0; (unsigned int)i < m_maxNumOutstandingTasks; i++)
	{
		m_taskBusy[i] = false;
	}
	m_numBusyTasks = 0;
	m_currentTask = 0;
	m_initialized = true;

	if (m_threadInterface->startSPU() != 0)
	{
		return -1;
	}
	return 0;
}



int SpursSpeexTaskManager::issueEncodeInitTask( int theQuality,int theGviSpeexSamplesPerSecond,  SpursSpeexTaskOutput *taskOutput, char *userAllocatedSpeexBuffer,int userAllocatedSpeexBufferSize )
{
	m_taskBusy[m_currentTask] = true;
	m_numBusyTasks++;

	SpursSpeexTaskDesc& taskDesc = mSpursSpeexTaskDesc[m_currentTask];
	taskDesc.mSpeexStateBuffer =  userAllocatedSpeexBuffer;
	taskDesc.mSpeexStateBufferSize =  userAllocatedSpeexBufferSize;

	taskDesc.mQuality = theQuality;
	taskDesc.mSamplesPerSecond = theGviSpeexSamplesPerSecond;
	taskDesc.mSpeexTaskOutput = taskOutput;
	if (issueTask(taskDesc,SPEEX_TASK_ENCODE_INIT_COMMAND) != 0)
		return -1;
	return 0;
}

int SpursSpeexTaskManager::issueEncodeTask(int16_t * inBuffer, int inBufferSize, int encodedFrameSize, char *outBuffer, 
										   int outBufferSize, SpursSpeexTaskOutput *taskOuput,char *userAllocatedSpeexBuffer,
										   int userAllocatedSpeexBufferSize )
{
	m_taskBusy[m_currentTask] = true;
	m_numBusyTasks++;

	SpursSpeexTaskDesc& taskDesc = mSpursSpeexTaskDesc[m_currentTask];
	taskDesc.mSpeexStateBuffer =  userAllocatedSpeexBuffer;
	taskDesc.mSpeexStateBufferSize =  userAllocatedSpeexBufferSize;
	taskDesc.mEncodedFrameSize = encodedFrameSize;
	taskDesc.mInputBuffer = inBuffer;
	taskDesc.mInputBufferSize = inBufferSize;
	taskDesc.mOutputBuffer = outBuffer;
	taskDesc.mOutputBufferSize = outBufferSize;
	taskDesc.mSpeexTaskOutput = 	taskOuput;
	if (issueTask(taskDesc,SPEEX_TASK_ENCODE_COMMAND) != 0)
		return -1;
	return 0;
}

int SpursSpeexTaskManager::issueDecodeAddTask(char *decoderStateBuffer, int decoderStateBufferSize, char *inBuffer, int inBufferSize, int encodedFrameSize,  
											  short* outBuffer, int outBufferSize, struct SpursSpeexTaskOutput *taskOutput)
{
	m_taskBusy[m_currentTask] = true;
	m_numBusyTasks++;

	SpursSpeexTaskDesc& taskDesc = mSpursSpeexTaskDesc[m_currentTask];
	taskDesc.mSpeexStateBuffer = decoderStateBuffer;
	taskDesc.mSpeexStateBufferSize = decoderStateBufferSize;
	taskDesc.mEncodedFrameSize = encodedFrameSize;
	taskDesc.mInputBuffer = inBuffer;
	taskDesc.mInputBufferSize = inBufferSize;
	taskDesc.mOutputBuffer = outBuffer;
	taskDesc.mOutputBufferSize = outBufferSize;
	taskDesc.mSpeexTaskOutput = taskOutput;
	if (issueTask(taskDesc,SPEEX_TASK_DECODEADD_COMMAND) != 0)
		return -1;
	return 0;
}

int SpursSpeexTaskManager::issueDecodeSetTask(char *decoderStateBuffer, int decoderStateBufferSize, char *inBuffer, int inBufferSize,  int encodedFrameSize,
											  short* outBuffer, int outBufferSize, struct SpursSpeexTaskOutput *taskOutput )
{
	m_taskBusy[m_currentTask] = true;
	m_numBusyTasks++;

	SpursSpeexTaskDesc& taskDesc = mSpursSpeexTaskDesc[m_currentTask];
	taskDesc.mSpeexStateBuffer = decoderStateBuffer;
	taskDesc.mSpeexStateBufferSize = decoderStateBufferSize;
	taskDesc.mEncodedFrameSize = encodedFrameSize;
	taskDesc.mInputBuffer = inBuffer;
	taskDesc.mInputBufferSize = inBufferSize;
	taskDesc.mOutputBuffer = outBuffer;
	taskDesc.mOutputBufferSize = outBufferSize;
	taskDesc.mSpeexTaskOutput = taskOutput;
	if (issueTask(taskDesc,SPEEX_TASK_DECODESET_COMMAND) !=  0)
		return -1;
	return 0;
}

int SpursSpeexTaskManager::issueDecodeInitTask( char *decoderStateBuffer, int decoderStateBufferSize, int sampleRate, struct SpursSpeexTaskOutput *taskOutput )
{
	m_taskBusy[m_currentTask] = true;
	m_numBusyTasks++;

	SpursSpeexTaskDesc& taskDesc = mSpursSpeexTaskDesc[m_currentTask];
	taskDesc.mSpeexStateBuffer = decoderStateBuffer;
	taskDesc.mSpeexStateBufferSize = decoderStateBufferSize;
	taskDesc.mSamplesPerSecond = sampleRate;
	taskDesc.mSpeexTaskOutput = taskOutput;
	if (issueTask(taskDesc,SPEEX_TASK_DECODE_INIT_COMMAND) != 0)
		return -1;
	return 0;
}


int SpursSpeexTaskManager::issueTask( SpursSpeexTaskDesc& taskDesc,uint32_t uiCommand )
{
#ifdef DEBUG_SPU_TASK_SCHEDULING
	printf("SpuSampleTaskProcess::issueTask (m_currentTask= %d\)n", m_currentTask);
#endif 

	taskDesc.mTaskId = m_currentTask;

	if (m_threadInterface->sendRequest(uiCommand, (uint32_t) &taskDesc, m_currentTask) != 0)
	{
		m_taskBusy[m_currentTask] = false;
		m_numBusyTasks--;
		return -1;
	}

	if (m_numBusyTasks >= m_maxNumOutstandingTasks)
	{
		unsigned int taskId;
		unsigned int outputSize;

		if (m_threadInterface->waitForResponse(&taskId, &outputSize) != 0)
		{
			return -2;
		}

		m_taskBusy[taskId] = false;
		m_numBusyTasks--;
	}

	for (unsigned int i = 0; i < m_maxNumOutstandingTasks; i++)
	{
		if (!m_taskBusy[i])
		{
			m_currentTask = i;
			break;
		}
	}
	return 0;
}



int SpursSpeexTaskManager::flush()
{
#ifdef DEBUG_SPU_TASK_SCHEDULING
	printf("\nSpuCollisionTaskProcess::flush()\n");
#endif 

	while(m_numBusyTasks > 0)
	{
		unsigned int taskId;
		unsigned int outputSize;

		if (m_threadInterface->waitForResponse(&taskId, &outputSize) != 0)
			return -1;
		m_taskBusy[taskId] = false;
		m_numBusyTasks--;
	}
	return 0;
}
#endif 
