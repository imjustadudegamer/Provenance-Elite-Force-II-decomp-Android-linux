/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"

#define	MAX_PORTALS	32768

#define	PORTALFILE	"PRT1"

#define	ON_EPSILON	0.1


#define SEPERATORCACHE

#define MAX_SEPERATORS		64

typedef struct
{
	vec3_t		normal;
	float		dist;
} plane_t;

#define MAX_POINTS_ON_WINDING	64
#define	MAX_POINTS_ON_FIXED_WINDING	12

typedef struct
{
	int		numpoints;
	vec3_t	points[MAX_POINTS_ON_FIXED_WINDING];			
} winding_t;

winding_t	*NewWinding (int points);
void		FreeWinding (winding_t *w);
winding_t	*CopyWinding (winding_t *w);


typedef struct passage_s
{
	struct passage_s	*next;
	byte				cansee[1];	
} passage_t;

typedef enum {stat_none, stat_working, stat_done} vstatus_t;
typedef struct
{
	int			num;
	qboolean	hint;	
	qboolean	removed;
	plane_t		plane;	
	int			leaf;	
	
	vec3_t		origin;	
	float		radius;

	winding_t	*winding;
	vstatus_t	status;
	byte		*portalfront;	
	byte		*portalflood;	
	byte		*portalvis;		

	int			nummightsee;	
	passage_t	*passages;		
} vportal_t;

#define	MAX_PORTALS_ON_LEAF		128
typedef struct leaf_s
{
	int			numportals;
	int			merged;
	vportal_t	*portals[MAX_PORTALS_ON_LEAF];
} leaf_t;

	
typedef struct pstack_s
{
	byte		mightsee[MAX_PORTALS/8];		
	struct pstack_s	*next;
	leaf_t		*leaf;
	vportal_t	*portal;	
	winding_t	*source;
	winding_t	*pass;

	winding_t	windings[3];	
	int			freewindings[3];

	plane_t		portalplane;
	int depth;
#ifdef SEPERATORCACHE
	plane_t seperators[2][MAX_SEPERATORS];
	int numseperators[2];
#endif
} pstack_t;

typedef struct
{
	vportal_t	*base;
	int			c_chains;
	pstack_t	pstack_head;
} threaddata_t;



extern	int			numportals;
extern	int			portalclusters;

extern	vportal_t	*portals;
extern	leaf_t		*leafs;

extern	int			c_portaltest, c_portalpass, c_portalcheck;
extern	int			c_portalskip, c_leafskip;
extern	int			c_vistest, c_mighttest;
extern	int			c_chains;

extern	byte	*vismap, *vismap_p, *vismap_end;	

extern	int			testlevel;

extern	byte		*uncompressed;

extern	int		leafbytes, leaflongs;
extern	int		portalbytes, portallongs;


void LeafFlow (int leafnum);


void BasePortalVis(int portalnum);
void BetterPortalVis(int portalnum);
void PortalFlow(int portalnum);
void PassagePortalFlow(int portalnum);
void CreatePassages(int portalnum);
void PassageFlow(int portalnum);

extern	vportal_t	*sorted_portals[MAX_MAP_PORTALS*2];

int CountBits (byte *bits, int numbits);
