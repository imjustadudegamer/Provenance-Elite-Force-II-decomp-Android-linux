#ifndef __CELL_UTILITY_MACROS_H
#define __CELL_UTILITY_MACROS_H

// This type pretends to be a pointer to a type on the PPU, and
#ifndef CELL_PPU_POINTER
#define CELL_PPU_PTR_TYPE uint32_t

#ifdef __SPU__
#define CELL_PPU_POINTER(x) CELL_PPU_PTR_TYPE
#else 
#define CELL_PPU_POINTER(x) x *

#endif 
#endif 

#endif
