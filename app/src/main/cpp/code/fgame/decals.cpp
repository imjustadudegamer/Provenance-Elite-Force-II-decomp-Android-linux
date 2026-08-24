

#include "decals.h"
#include "level.h"

#define R120_PROBE_GET(name, def, flags) gi.cvar(name, def, flags)
#define R120_PROBE_PRINT                 gi.Printf
#include "../qcommon/qa_r120.h"

static int r133_decnorm_n = 0;

CLASS_DECLARATION( Entity, Decal, NULL )
	{
		{ NULL, NULL }
	};

Decal::Decal
   (
   )

   {
   edict->s.eType = ET_DECAL;
   edict->s.modelindex  = 1;	      
   PostEvent( EV_Remove, FRAMETIME );
   }

void Decal::setDirection
   (
   Vector dir
   )

   {
   if (R120_On() && R120_Throttle(&r133_decnorm_n, 1, 32)) {
       trace_t *t = &level.impact_trace;
       R133_PROBE(
           "DECNORM",
           "ent=%d shader='%s' raw=(%.4f,%.4f,%.4f) |raw|=%.4f byte=%d "
           "tsolid=%d tstart=%d tfrac=%.4f tent=%d tend=(%.1f,%.1f,%.1f) tplane=(%.4f,%.4f,%.4f)\n",
           edict ? edict->s.number : -1,
           shader.c_str(),
           dir[0], dir[1], dir[2],
           dir.length(),
           DirToByte(dir),
           t->allsolid ? 1 : 0,
           t->startsolid ? 1 : 0,
           t->fraction,
           t->ent ? t->ent->s.number : -1,
           t->endpos[0], t->endpos[1], t->endpos[2],
           t->plane.normal[0], t->plane.normal[1], t->plane.normal[2]);
   }

   edict->s.surfaces[0] = DirToByte( dir );
   }

void Decal::setShader
   (
   str decal_shader
   )

   {
   str temp_shader;

   shader = decal_shader;
   edict->s.tag_num = gi.imageindex( shader.c_str() );

	temp_shader = shader + ".spr";
	CacheResource( temp_shader, this );
   }

void Decal::setOrientation
   (
   str deg
   )

   {
   Vector ang;

   if ( !deg.icmp( "random" ) )
      ang[2] = random() * 360;
   else
      ang[2] = atof( deg );   

   setAngles( ang );
   }

void Decal::setRadius 
   (
   float rad
   )

   {
   edict->s.scale = rad;
   }

