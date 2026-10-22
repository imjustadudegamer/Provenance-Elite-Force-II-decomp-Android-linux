/*
===========================================================================
Copyright (C) 2023 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later


#include "q_shared.h"
#include "qcommon.h"
#include "tiki_imports.h"

#define PROBE_GET(name, def, flags) Cvar_Get(name, def, flags)
#define PROBE_PRINT                 Com_Printf
#include "../qcommon/qa_probe.h"

#ifndef DEDICATED
#include "../client/client.h"
#endif

void TIKI_DPrintf(const char *fmt, ...)
{
    char    msg[4096];
    va_list va;

    va_start(va, fmt);
    Q_vsnprintf(msg, sizeof(msg), fmt, va);
    va_end(va);

    Com_DPrintf("%s", msg);
}

void TIKI_Free(void *ptr)
{
    Z_Free(ptr);
}

void *TIKI_Alloc(size_t size)
{
    return Z_TagMalloc(size, TAG_TIKI);
}

void TIKI_FreeFile(void *buffer)
{
    FS_FreeFile(buffer);
}

int TIKI_ReadFileEx(const char *qpath, void **buffer, qboolean quiet)
{
    return FS_ReadFileEx(qpath, buffer, quiet);
}

void TIKI_UpdateScreen(void)
{
#ifndef DEDICATED
    SCR_UpdateScreen();
#endif
}

/* ---- [TIKI:skel] the cold-animation split --------------------------------- A cold character
   tiki costs 0.3-1.3 s of a restore and the only thing the model-leg row can say today is the
   total. Every animation the skeletor cache loads comes through here, so the read (bytes, through
   the compressed paks and the OBB's FUSE mount) and the convert (the channel/frame work) are
   charged apart, and the roll prints once a second while a load is running. Which of the two owns
   the second decides the optimisation: a read leg wants fewer or larger reads, a convert leg wants
   parallelism or a cached conversion. */
typedef struct {
    int      loads;
    int      bytes;
    int      readMs;
    int      convMs;
    int      distinct;
    int      repeats;
    int      rollAt;
} tikiSkelRoll_t;

/* Paths this process already paged in, by name hash.  A collision can hide a
   repeat and never invent one, so repeat= is a lower bound. */
#define TIKI_SKEL_PATH_BUCKETS 8192

static unsigned int tikiSkelPathSeen[TIKI_SKEL_PATH_BUCKETS];

static unsigned long long TIKI_SkelPathHash( const char *path )
{
    unsigned long long h = 1469598103934665603ull;

    while ( *path ) {
        h ^= (unsigned char)*path++;
        h *= 1099511628211ull;
    }

    h = ( h >> 32 ) ^ h;

    return h ? h : 1;
}

/* qtrue when this path was paged in before; a first path is claimed here. */
static qboolean TIKI_SkelPathRepeat( const char *path )
{
    unsigned int hash = (unsigned int)TIKI_SkelPathHash( path );
    unsigned int slot = hash & ( TIKI_SKEL_PATH_BUCKETS - 1 );
    int          probe;

    for ( probe = 0; probe < TIKI_SKEL_PATH_BUCKETS; probe++ ) {
        unsigned int at = ( slot + (unsigned int)probe ) & ( TIKI_SKEL_PATH_BUCKETS - 1 );

        if ( !tikiSkelPathSeen[at] ) {
            tikiSkelPathSeen[at] = hash;
            return qfalse;
        }

        if ( tikiSkelPathSeen[at] == hash ) {
            return qtrue;
        }
    }

    return qtrue;
}

static tikiSkelRoll_t s_tikiSkel;

int TIKI_SkelRollStart( void )
{
    s_tikiSkel.loads++;
    return Com_Milliseconds();
}

void TIKI_SkelRollRead( int bytes, int usec, const char *path )
{
    s_tikiSkel.bytes += bytes;
    s_tikiSkel.readMs += usec;

    /* The processed-anim pre-read reports zero by design: retail ships no
       newanim/ directory (0 entries across the shipped paks), so a zero-length
       first read is the fallback arm's entry, not a miss.  What the row needs
       is whether this boot reads a path it already read. */
    if ( bytes > 0 && path && path[0] ) {
        if ( TIKI_SkelPathRepeat( path ) ) {
            s_tikiSkel.repeats++;
        } else {
            s_tikiSkel.distinct++;
        }
    }

    if ( !s_tikiSkel.rollAt ) {
        s_tikiSkel.rollAt = Com_Milliseconds();
    }
}

void TIKI_SkelRollEnd( int t0 )
{
    int now = Com_Milliseconds();

    s_tikiSkel.convMs += now - t0;

    if ( !s_tikiSkel.rollAt || now - s_tikiSkel.rollAt < 1000 || !PROBE_On() ) {
        return;
    }
    Com_Printf( "[TIKI:skel] loads=%d bytes=%d read=%dms conv=%dms distinct=%d repeat=%d win=%dms\n",
                s_tikiSkel.loads, s_tikiSkel.bytes, s_tikiSkel.readMs, s_tikiSkel.convMs,
                s_tikiSkel.distinct, s_tikiSkel.repeats, now - s_tikiSkel.rollAt );
    memset( &s_tikiSkel, 0, sizeof( s_tikiSkel ) );
}

/* ---- [TIKI:tiki] the registration half of a cold setmodel ----------------- The restore's model
   legs are 5.6 s and the engine call that owns them is gi.SetModel. This splits the two jobs
   inside it -- the .tik parse/register and the model build -- so the skel roll above (the
   animation paging) and this row together account for the whole leg. */
typedef struct {
    int regs;
    int parseMs;
    int buildMs;
    int warm;
    int rollAt;
} tikiTikiRoll_t;

static tikiTikiRoll_t s_tikiTiki;

int TIKI_Millis( void )
{
    return Com_Milliseconds();
}

void TIKI_TikiRollParse( int ms )
{
    s_tikiTiki.parseMs += ms;
    if ( !s_tikiTiki.rollAt ) {
        s_tikiTiki.rollAt = Com_Milliseconds();
    }
}

void TIKI_TikiRollBuild( int ms )
{
    s_tikiTiki.regs++;
    s_tikiTiki.buildMs += ms;
    if ( !s_tikiTiki.rollAt ) {
        s_tikiTiki.rollAt = Com_Milliseconds();
    }
}

void TIKI_TikiRollWarm( void )
{
    s_tikiTiki.warm++;
    if ( !s_tikiTiki.rollAt ) {
        s_tikiTiki.rollAt = Com_Milliseconds();
    }
}

void TIKI_TikiRoll( void )
{
    int now = Com_Milliseconds();

    if ( !s_tikiTiki.rollAt || now - s_tikiTiki.rollAt < 1000 || !PROBE_On() ) {
        return;
    }
    Com_Printf( "[TIKI:tiki] regs=%d parse=%dms build=%dms warm=%d win=%dms\n",
                s_tikiTiki.regs, s_tikiTiki.parseMs, s_tikiTiki.buildMs,
                s_tikiTiki.warm, now - s_tikiTiki.rollAt );
    memset( &s_tikiTiki, 0, sizeof( s_tikiTiki ) );
}

/* the keep-loaded arm retained this many models at an unload */
void TIKI_TikiRollKeep( int kept )
{
    if ( !PROBE_On() ) {
        return;
    }
    Com_Printf( "[TIKI:keep] kept=%d\n", kept );
}
