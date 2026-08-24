/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#include "c.h"
#define I(f) null_##f

static Node I(gen)(Node p) { return p; }
static void I(address)(Symbol q, Symbol p, long n) {}
static void I(blockbeg)(Env *e) {}
static void I(blockend)(Env *e) {}
static void I(defaddress)(Symbol p) {}
static void I(defconst)(int suffix, int size, Value v) {}
static void I(defstring)(int len, char *s) {}
static void I(defsymbol)(Symbol p) {}
static void I(emit)(Node p) {}
static void I(export)(Symbol p) {}
static void I(function)(Symbol f, Symbol caller[], Symbol callee[], int ncalls) {}
static void I(global)(Symbol p) {}
static void I(import)(Symbol p) {}
static void I(local)(Symbol p) {}
static void I(progbeg)(int argc, char *argv[]) {}
static void I(progend)(void) {}
static void I(segment)(int s) {}
static void I(space)(int n) {}
static void I(stabblock)(int brace, int lev, Symbol *p) {}
static void I(stabend)(Coordinate *cp, Symbol p, Coordinate **cpp, Symbol *sp, Symbol *stab) {}
static void I(stabfend)(Symbol p, int lineno) {}
static void I(stabinit)(char *file, int argc, char *argv[]) {}
static void I(stabline)(Coordinate *cp) {}
static void I(stabsym)(Symbol p) {}
static void I(stabtype)(Symbol p) {}


Interface nullIR = {
	{1, 1, 0},	
	{2, 2, 0},	
	{4, 4, 0},	
	{8, 8, 1},	
	{8 ,8, 1},	
	{4, 4, 1},	
	{8, 8, 1},	
	{16,16,1},	
	{4, 4, 0},	
	{0, 4, 0},	
	1,		
	0,		
	0,		
	0,		
	1,		
	0,		
	0,		
	I(address),
	I(blockbeg),
	I(blockend),
	I(defaddress),
	I(defconst),
	I(defstring),
	I(defsymbol),
	I(emit),
	I(export),
	I(function),
	I(gen),
	I(global),
	I(import),
	I(local),
	I(progbeg),
	I(progend),
	I(segment),
	I(space),
	I(stabblock),
	I(stabend),
	I(stabfend),
	I(stabinit),
	I(stabline),
	I(stabsym),
	I(stabtype)
};
