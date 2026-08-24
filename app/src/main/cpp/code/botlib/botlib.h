
#ifndef __BOTLIB_SHIM_H__
#define __BOTLIB_SHIM_H__

#include "../fgame/botlib.h"

#define g_gametype        botlib_g_gametype
#define CRC_Init          botlib_CRC_Init
#define CRC_ProcessByte   botlib_CRC_ProcessByte
#define CRC_Value         botlib_CRC_Value
#define crctable          botlib_crctable

// dedicated "jumped on the previous frame" latch: retail reuses ACTION_CROUCH

#endif
