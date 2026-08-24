/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#ifndef __SPU_ASSERT_H__
#define __SPU_ASSERT_H__


#ifdef _DEBUG

#ifdef __CELLOS_LV2__
#include <spu_printf.h>
#define SPU_ASSERT(cond) do { if (__builtin_expect(!(cond), 0)) { spu_printf("SPU: Assertion failed!  Expression: " #cond "\n    in %s at " __FILE__ ":%i\n", __FUNCTION__, __LINE__); spu_hcmpeq((cond), 0); } } while (0)
#else 
#define SPU_ASSERT(cond) assert(cond)
#endif 

#else  

#ifdef __CELLOS_LV2__
#include <spu_printf.h>
#define SPU_ASSERT(cond) do { if (__builtin_expect(!(cond), 0)) { spu_printf("SPU: Assertion failed!  Expression: " #cond "\n    in %s at " __FILE__ ":%i\n", __FUNCTION__, __LINE__); spu_hcmpeq((cond), 0); } } while (0)
#else  
#define SPU_ASSERT(cond)  assert(cond)
#endif 

#endif 

#define SPU_COMPILE_TIME_ASSERT(cond) extern char spu_compile_time_assert_failed[cond ? 1 : -1]

#define SPU_NAMED_COMPILE_TIME_ASSERT(name, cond) extern char name[cond ? 1 : -1]


#endif
