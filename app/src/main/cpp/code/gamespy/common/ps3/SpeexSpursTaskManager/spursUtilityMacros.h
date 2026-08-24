/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#ifndef __CELL_UTILITY_MACROS_H
#define __CELL_UTILITY_MACROS_H

#ifndef CELL_PPU_POINTER
#define CELL_PPU_PTR_TYPE uint32_t

#ifdef __SPU__
#define CELL_PPU_POINTER(x) CELL_PPU_PTR_TYPE
#else 
#define CELL_PPU_POINTER(x) x *

#endif 
#endif 

#endif
