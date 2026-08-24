
#pragma once


#if defined( GAME_DLL )

#define glbs gi

#include "g_local.h"
#include "../qcommon/qcommon.h"

#else

#if defined( CGAME_DLL )

#define glbs cgi

#endif

#endif

// EF2 port: these shared headers must be visible in EVERY context (the retail
#include "../qcommon/q_shared.h"

#include "../corepp/Linklist.h"
#include "../corepp/mem_blockalloc.h"
#include "../corepp/vector.h"
#include <str.h>
#include <container.h>
#include <const_str.h>
#include <short3.h>
#include "../corepp/con_set.h"
#include <con_arrayset.h>
#include <scriptexception.h>
#include "../corepp/class.h"
#include <containerclass.h>
#include <stack.h>
#include "../corepp/listener.h"
#include <script.h>

#include "../qcommon/qcommon.h"

#if !defined( GAME_DLL )

#if defined ( CGAME_DLL )

#if defined ( CGAME_HOOK )

#include "../cgame_hook/cgamex86.h"

#endif

#endif

#endif
