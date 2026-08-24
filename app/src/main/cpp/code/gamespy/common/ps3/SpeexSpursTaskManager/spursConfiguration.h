/* [SCE CONFIDENTIAL DOCUMENT]
 * PLAYSTATION(R)3 SPU Optimized Bullet Physics Library (http://bulletphysics.com)
 *                Copyright (C) 2007 Sony Computer Entertainment Inc.
 *                                                All Rights Reserved.
 */

#ifndef __CELL_SPU_CONFIG
#define __CELL_SPU_CONFIG

#include <cell/spurs.h>

#ifdef __cplusplus
extern "C" { 
#endif

#define CELL_SPURS_DEFAULT_SPU_COUNT 1


enum CellSpursReturn {
	CELL_SPURS_OK=0,
	CELL_SPURS_EBUSY,
	CELL_SPURS_EINVAL,
	CELL_SPURS_EMISC
};


int spursConfiguration_initWithSpurs(CellSpurs *pSpurs, int iSPUCount, uint8_t auiPriorities[8]);

int spursConfiguration_initWithSpuCount(int iSPUCount);

int spursConfiguration_terminate();

bool spursConfiguration_isSpursInitialized();

#ifdef __cplusplus
}
#endif

#endif 
