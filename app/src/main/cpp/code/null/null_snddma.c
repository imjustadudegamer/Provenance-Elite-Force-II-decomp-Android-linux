

#include "../client/client.h"

qboolean SNDDMA_Init(void)
{
	return qfalse;
}

int	SNDDMA_GetDMAPos(void)
{
	return 0;
}

void SNDDMA_Shutdown(void)
{
}

void SNDDMA_BeginPainting (void)
{
}

void SNDDMA_Submit(void)
{
}

#ifdef USE_VOIP
#endif

void SNDDMA_Activate(void) {
}

void S_ClearSoundBuffer( void ) {
}

int S_IsSoundPlaying( int channel_number, const char *sfxName ){
	return 0;
}

#if defined(NO_MODERN_DMA) && NO_MODERN_DMA
void S_StartLocalSound(sfxHandle_t sfx, int channelNum) {
}
sfxHandle_t S_RegisterSound(const char* sample, qboolean compressed) {
	return 0;
}
#else
void S_StartLocalSound(const char* sound_name, qboolean force_load) {
}
sfxHandle_t S_RegisterSound(const char* name) {
	return 0;
}
#endif
