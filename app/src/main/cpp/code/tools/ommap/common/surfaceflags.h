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



#define	CONTENTS_SOLID			1		
#define	CONTENTS_LAVA			8
#define	CONTENTS_SLIME			16
#define	CONTENTS_WATER			32
#define	CONTENTS_FOG			64

#define	CONTENTS_AREAPORTAL		0x8000

#define	CONTENTS_PLAYERCLIP		0x10000
#define	CONTENTS_MONSTERCLIP	0x20000
#define	CONTENTS_TELEPORTER		0x40000
#define	CONTENTS_JUMPPAD		0x80000
#define CONTENTS_CLUSTERPORTAL	0x100000
#define CONTENTS_DONOTENTER		0x200000

#define	CONTENTS_ORIGIN			0x1000000	

#define	CONTENTS_BODY			0x2000000	
#define	CONTENTS_CORPSE			0x4000000
#define	CONTENTS_DETAIL			0x8000000	
#define	CONTENTS_STRUCTURAL		0x10000000	
#define	CONTENTS_TRANSLUCENT	0x20000000	
#define	CONTENTS_TRIGGER		0x40000000
#define	CONTENTS_NODROP			0x80000000	

#define	SURF_NODAMAGE			0x1		
#define	SURF_SLICK				0x2		
#define	SURF_SKY				0x4		
#define	SURF_LADDER				0x8
#define	SURF_NOIMPACT			0x10	
#define	SURF_NOMARKS			0x20	
#define	SURF_FLESH				0x40	
#define	SURF_NODRAW				0x80	
#define	SURF_HINT				0x100	
#define	SURF_SKIP				0x200	
#define	SURF_NOLIGHTMAP			0x400	
#define	SURF_POINTLIGHT			0x800	
#define	SURF_METALSTEPS			0x1000	
#define	SURF_NOSTEPS			0x2000	
#define	SURF_NONSOLID			0x4000	
#define SURF_LIGHTFILTER		0x8000	
#define	SURF_ALPHASHADOW		0x10000	
#define	SURF_NODLIGHT			0x20000	
