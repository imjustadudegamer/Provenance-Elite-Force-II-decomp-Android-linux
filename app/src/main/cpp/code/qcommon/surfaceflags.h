//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/surfaceflags.h                                $
// $Revision:: 12                                                             $
//   $Author:: Steven                                                         $
//     $Date:: 10/13/03 9:42a                                                 $
//
// Copyright (C) 1999 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION: Surface flag parameters for q3map and game
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#ifndef __SURFACEFLAGS_H__
#define __SURFACEFLAGS_H__




#define	CONTENTS_SOLID			   (1<<0)		
#define	CONTENTS_WINDOW				(1<<1)		
#define	CONTENTS_AUX				(1<<2)		
#define	CONTENTS_LAVA			   (1<<3)
#define	CONTENTS_SLIME			   (1<<4)
#define	CONTENTS_WATER			   (1<<5)
#define	CONTENTS_FOG			   (1<<6)

#define CONTENTS_MIST				(1<<6)		
#define CONTENTS_LADDER				(1<<7) 
#define CONTENTS_NOTTEAM2			(1<<8) 
#define CONTENTS_NOTTEAM1			(1<<9)
#define CONTENTS_JUMPPAD			(1<<10)
#define CONTENTS_TELEPORTER			(1<<11)
#define CONTENTS_MOVER				(1<<12)

#define CONTENTS_USABLE				(1<<12)
#define CONTENTS_SETCLIP			(1<<13)	
#define  CONTENTS_TARGETABLE		(1<<14)
#define  CONTENTS_AREAPORTAL     (1<<15)
#define	CONTENTS_PLAYERCLIP		(1<<16)
#define	CONTENTS_MONSTERCLIP	   (1<<17)
#define	CONTENTS_CAMERACLIP		(1<<18)
#define	CONTENTS_WEAPONCLIP		(1<<19)  
#define  CONTENTS_SHOOTABLE_ONLY (1<<20)  
#define CONTENTS_CLUSTERPORTAL (1 << 21) 
#define CONTENTS_BOTCLIP		(1 << 22) 
#define CONTENTS_DONOTENTER		(1 << 23) 
#define	CONTENTS_ORIGIN			(1<<24) 	
#define	CONTENTS_BODY			   (1<<25)	
#define	CONTENTS_CORPSE			(1<<26)
#define	CONTENTS_DETAIL			(1<<27)	
#define	CONTENTS_STRUCTURAL		(1<<28)	
#define	CONTENTS_TRANSLUCENT   (1<<29)	
#define CONTENTS_NOBOTCLIP		(1<<30)	
#define	CONTENTS_NODROP			(1<<31)	

#define  CONTENTS_KEEP  (CONTENTS_DETAIL)

#define	SURF_NODAMAGE			( 1<<0 )	
#define	SURF_SLICK				( 1<<1 )	
#define	SURF_SKY				( 1<<2 )	
#define	SURF_LADDER				( 1<<3 ) 
#define	SURF_NOIMPACT			( 1<<4 ) 
#define	SURF_NOMARKS			( 1<<5 ) 
#define SURF_CASTSHADOW			( 1<<6 ) 
#define SURF_NODRAW				( 1<<7 ) 
#define SURF_SKIP				( 1<<8 ) 
#define SURF_TYPE_SAND			( 1<<9 ) 
#define	SURF_NOLIGHTMAP			( 1<<10 )
#define SURF_ALPHASHADOW		( 1<<11 )
#define SURF_TYPE_SNOW			( 1<<12 )
#define	SURF_NOSTEPS			( 1<<13 )
#define	SURF_NONSOLID			( 1<<14 )
#define SURF_RICOCHET			( 1<<15 )

#define SURF_TYPE_METAL_DUCT	( 1<<16 )
#define SURF_TYPE_METAL			( 1<<17 )
#define SURF_TYPE_ROCK			( 1<<18 )
#define SURF_TYPE_DIRT			( 1<<19 )
#define SURF_TYPE_GRILL			( 1<<20 )
#define SURF_TYPE_ORGANIC		( 1<<21 )
#define SURF_TYPE_SQUISHY		( 1<<22 )

#define	SURF_NODLIGHT			( 1<<23 )
#define	SURF_HINT   			( 1<<24 )
#define SURF_TYPE_DEFAULT		( 1<<25 )

#define SURF_TYPE_METAL_HOLLOW	( 1<<26 )
#define SURF_TYPE_CARPET		( 1<<27 )

#define SURF_PATCH				( 1<<29 )
#define SURF_TERRAIN			( 1<<30 )

#define SURF_KEEP		(SURF_PATCH | SURF_TERRAIN)

#define MASK_SURF_TYPE	(SURF_TYPE_SAND|SURF_TYPE_SNOW|SURF_TYPE_METAL_DUCT|SURF_TYPE_METAL|SURF_TYPE_ROCK|SURF_TYPE_DIRT|SURF_TYPE_GRILL|SURF_TYPE_ORGANIC|SURF_TYPE_SQUISHY|SURF_TYPE_METAL_HOLLOW|SURF_TYPE_CARPET)


#define CONTENTS_TRIGGER      CONTENTS_CLUSTERPORTAL  

enum
{
	TRACE_IGNORE_CURVE_PATCHES	=	( 1 << 0 ),
	TRACE_IGNORE_TERRAIN_BRUSHES =	( 1 << 1 ),
};

#endif	
