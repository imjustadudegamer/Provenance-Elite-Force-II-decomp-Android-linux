/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#ifndef __CELL_VECTORMATH_H__
#define __CELL_VECTORMATH_H__

#ifndef __CELLOS_LV2__
class vec_float4
{
	float x, y, z, w;
} 
#ifdef __GNUC__
__attribute__ ((aligned (16)));
#else
__declspec(align(16));
#endif 
#endif 

#ifdef __CELLOS_LV2__
#include "vectormath_soa.h"
#else
#include "vectormath_scalar/vectormath_aos.h"
#endif 

using namespace Vectormath;
using namespace Vectormath::Aos;

#endif 
