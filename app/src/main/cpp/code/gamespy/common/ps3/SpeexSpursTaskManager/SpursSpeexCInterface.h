#ifndef SPU_TASK_C_INTERFACE_H
#define SPU_TASK_C_INTERFACE_H

#include "SpuSpeexTaskOutput.h"

#define PL_DECLARE_HANDLE(name) typedef struct name##__ { int unused; } *name

typedef float	plReal;
typedef plReal	plVector3[3];
typedef plReal	plQuaternion[4];

#ifdef __cplusplus
extern "C" { 
#endif

#define GVI_REMAINING_BYTES 3


int initializeSpursSampleTask();

int issueSampleTaskEncode(short* inBuffer, int inBufferSize, int encodedFrameSize,  char *outBuffer, int outBufferSize, 
                          struct SpursSpeexTaskOutput *taskOuput, char *userAllocatedSpeexBuffer, 
                          int userAllocatedSpeexBufferSize);

int issueSampleTaskEncodeInit(int quality, int samplesPerFrame, struct SpursSpeexTaskOutput *taskOutput,
                              char *userAllocatedSpeexBuffer, int userAllocatedSpeexBufferSize);
int issueSampleTaskDecodeAdd(char *decoderStateBuffer, int decoderStateBufferSize, char *inBuffer, int inBufferSize, int encodedFrameSize,
							 short* outBuffer, int outBufferSize, struct SpursSpeexTaskOutput *taskOutput);
int issueSampleTaskDecodeSet(char *decoderStateBuffer, int decoderStateBufferSize, char *inBuffer, int inBufferSize, int encodedFrameSize,
							 short* outBuffer, int outBufferSize, struct SpursSpeexTaskOutput *taskOutput);
int issueSampleTaskDecodeInit(char *decoderStateBuffer, int decoderStateBufferSize, int sampleRate, struct SpursSpeexTaskOutput *taskOutput);





int shutdownSpursTask();


#ifdef __cplusplus
}
#endif

#endif 
