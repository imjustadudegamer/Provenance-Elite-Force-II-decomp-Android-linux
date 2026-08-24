
#include "qasm-inline.h"

#if defined (__i386__) || defined(__x86_64__)
static const unsigned short fpucw = 0x0C7F;


long qftolsse(float f)
{
  long retval;
  
  __asm__ volatile
  (
    "cvttss2si %1, %0\n"
    : "=r" (retval)
    : "x" (f)
  );
  
  return retval;
}

int qvmftolsse(void)
{
  int retval;
  
  __asm__ volatile
  (
    "movss (" EDI ", " EBX ", 4), %%xmm0\n"
    "cvttss2si %%xmm0, %0\n"
    : "=r" (retval)
    :
    : "%xmm0"
  );
  
  return retval;
}

long qftolx87(float f)
{
  long retval;
  unsigned short oldcw = 0;

  __asm__ volatile
  (
    "fnstcw %2\n"
    "fldcw %3\n"
    "flds %1\n"
    "fistpl %1\n"
    "fldcw %2\n"
    "mov %1, %0\n"
    : "=r" (retval)
    : "m" (f), "m" (oldcw), "m" (fpucw)
  );
  
  return retval;
}

int qvmftolx87(void)
{
  int retval;
  unsigned short oldcw = 0;

  __asm__ volatile
  (
    "fnstcw %1\n"
    "fldcw %2\n"
    "flds (" EDI ", " EBX ", 4)\n"
    "fistpl (" EDI ", " EBX ", 4)\n"
    "fldcw %1\n"
    "mov (" EDI ", " EBX ", 4), %0\n"
    : "=r" (retval)
    : "m" (oldcw), "m" (fpucw)
  );
  
  return retval;
}
#endif
