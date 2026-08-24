/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#include "c.h"
extern Interface nullIR;
extern Interface bytecodeIR;
Binding bindings[] = {
	{ "null",          &nullIR },
	{ "bytecode",      &bytecodeIR },
	{ NULL,            NULL },
};
