/*
 *  This is a modified version of Mark Adlers work,
 *  see below for the original copyright.
 *  2006 - Joerg Dietrich <dietrich_joerg@gmx.de>
 */


#ifndef __PUFF_H
#define __PUFF_H

#include "../qcommon/q_shared.h"			

int32_t puff(uint8_t  *dest,		
             uint32_t *destlen,		
             uint8_t  *source,		
             uint32_t *sourcelen);	

#endif 
