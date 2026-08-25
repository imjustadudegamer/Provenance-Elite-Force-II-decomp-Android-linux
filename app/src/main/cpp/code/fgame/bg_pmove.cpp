
#include "../qcommon/q_shared.h"
#include "bg_public.h"



typedef struct {
	vec3_t		forward, left, up;
	vec3_t		flat_forward, flat_left, flat_up;
	float		frametime;

	int			msec;

	qboolean	walking;
	qboolean	groundPlane;
	trace_t		groundTrace;

	float		impactSpeed;

	vec3_t		previous_origin;
	vec3_t		previous_velocity;
	int			previous_waterlevel;
} pml_t;

pmove_t	*pm;
pml_t		pml;
qboolean slopeSlideFlag = qfalse;

const float	pm_swimScale         = 0.50f;
[[maybe_unused]] const float	pm_wadeScale         = 0.70f;
const float	pm_slipperyfriction  = 0.25f;
const float pm_leaninspeed		 = 75.0f;
const float pm_leanoutspeed		 = 150.0f;
const float	pm_leanmaxdelta		 = 35.0f;

int c_pmove = 0;

void PM_CrashLand( void );
qboolean PM_ShouldCrashLand( void );

void PM_AddTouchEnt
   (
   int entityNum
   )

   {
	int i;

	if ( entityNum == ENTITYNUM_WORLD )
      {
		return;
	   }

	if ( pm->numtouch == MAXTOUCH )
      {
		return;
	   }

	for( i = 0; i < pm->numtouch; i++ )
      {
		if ( pm->touchents[ i ] == entityNum )
         {
			return;
		   }
	   }

	pm->touchents[ pm->numtouch ] = entityNum;
	pm->numtouch++;
   }

void PM_ClipVelocity
   (
   const vec3_t in,
   const vec3_t normal,
   vec3_t out,
   float overbounce
   )

   {
	float	backoff;
	float	change;
	int	i;

	backoff = DotProduct( in, normal );

	if ( backoff < 0.0f )
      {
		backoff *= overbounce;
	   }
   else
      {
		backoff /= overbounce;
	   }

	for( i = 0; i < 3; i++ )
      {
		change = normal[ i ] * backoff;
		out[ i ] = in[ i ] - change;
	   }
   }

#define	MAX_CLIP_PLANES	5
qboolean	PM_SlideMove( qboolean gravity ) {
	int			bumpcount, numbumps;
	vec3_t		dir;
	float		d;
	int			numplanes;
	vec3_t		planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity;
	vec3_t		clipVelocity;
	int			i, j, k;
	trace_t	trace;
	vec3_t		end;
	float		time_left;
	float		into;
	vec3_t		endVelocity;
	vec3_t		endClipVelocity;
	int gravtmp;

	numbumps = 4;

	VectorCopy (pm->ps->velocity, primal_velocity);


	if ( gravity ) {
		if ( pm->ps->pm_flags & PMF_FLIGHT )
			gravtmp = 0;
		else
			gravtmp = pm->ps->gravity;

		VectorCopy( pm->ps->velocity, endVelocity );
		endVelocity[2] -= gravtmp * pml.frametime;
		pm->ps->velocity[2] = ( pm->ps->velocity[2] + endVelocity[2] ) * 0.5f;
		primal_velocity[2] = endVelocity[2];
		if ( pml.groundPlane ) {
			PM_ClipVelocity (pm->ps->velocity, pml.groundTrace.plane.normal,
				pm->ps->velocity, OVERCLIP );
		}
	}

	time_left = pml.frametime;

	if ( pml.groundPlane ) {
		numplanes = 1;
		VectorCopy( pml.groundTrace.plane.normal, planes[0] );
	} else {
		numplanes = 0;
	}

	VectorNormalize2( pm->ps->velocity, planes[numplanes] );
	numplanes++;

	for ( bumpcount=0 ; bumpcount < numbumps ; bumpcount++ ) {

		VectorMA( pm->ps->origin, time_left, pm->ps->velocity, end );


		pm->trace ( &trace, pm->ps->origin, pm->mins, pm->maxs, end, pm->ps->clientNum, pm->tracemask, qtrue );

      if ( trace.startsolid && trace.entityNum != ENTITYNUM_WORLD )
         {
         pm->trace ( &trace, pm->ps->origin, pm->mins, pm->maxs, end, trace.entityNum, pm->tracemask, qtrue );
         }

      if (trace.allsolid) {
			pm->ps->velocity[2] = 0.0f;	
			return qtrue;
		}

      if ( pm->trypush && pm->trypush( trace.entityNum, pm->ps->origin, end ) )
			continue;

		if (trace.fraction > 0.0f) {
			VectorCopy (trace.endpos, pm->ps->origin);
		}

		if (trace.fraction == 1.0f) {
			 break;		
		}


		PM_AddTouchEnt( trace.entityNum );

      time_left -= time_left * trace.fraction;

		if (numplanes >= MAX_CLIP_PLANES) {
			VectorClear( pm->ps->velocity );
			return qtrue;
		}

		for ( i = 0 ; i < numplanes ; i++ ) {
			if ( DotProduct( trace.plane.normal, planes[i] ) > 0.99f ) {
				VectorAdd( trace.plane.normal, pm->ps->velocity, pm->ps->velocity );
				break;
			}
		}
		if ( i < numplanes ) {
			continue;
		}
		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;


		for ( i = 0 ; i < numplanes ; i++ ) {
			into = DotProduct( pm->ps->velocity, planes[i] );
			if ( into >= 0.1f ) {
				continue;		
			}

			if ( -into > pml.impactSpeed ) {
				pml.impactSpeed = -into;
			}

			PM_ClipVelocity (pm->ps->velocity, planes[i], clipVelocity, OVERCLIP );

			PM_ClipVelocity (endVelocity, planes[i], endClipVelocity, OVERCLIP );

			for ( j = 0 ; j < numplanes ; j++ ) {
				if ( j == i ) {
					continue;
				}
				if ( DotProduct( clipVelocity, planes[j] ) >= 0.1f ) {
					continue;		
				}

				PM_ClipVelocity( clipVelocity, planes[j], clipVelocity, OVERCLIP );
				PM_ClipVelocity( endClipVelocity, planes[j], endClipVelocity, OVERCLIP );

				if ( DotProduct( clipVelocity, planes[i] ) >= 0.0f ) {
					continue;
				}

				CrossProduct (planes[i], planes[j], dir);
				VectorNormalize( dir );
				d = DotProduct( dir, pm->ps->velocity );
				VectorScale( dir, d, clipVelocity );

				CrossProduct (planes[i], planes[j], dir);
				VectorNormalize( dir );
				d = DotProduct( dir, endVelocity );
				VectorScale( dir, d, endClipVelocity );

				for ( k = 0 ; k < numplanes ; k++ ) {
					if ( k == i || k == j ) {
						continue;
					}
					if ( DotProduct( clipVelocity, planes[k] ) >= 0.1f ) {
						continue;		
					}

					VectorClear( pm->ps->velocity );
					return qtrue;
				}
			}

			VectorCopy( clipVelocity, pm->ps->velocity );
			VectorCopy( endClipVelocity, endVelocity );
			break;
		}
	}

	if ( gravity ) {
		VectorCopy( endVelocity, pm->ps->velocity );
	}

	if ( pm->ps->pm_time ) {
		VectorCopy( primal_velocity, pm->ps->velocity );
	}

	return ( bumpcount != 0 );
}

float PM_TryStepSlideMove( qboolean gravity, float stepSize )
{
	trace_t		trace;
	vec3_t		up, down;
	vec3_t		diff;
	float		distance;

	VectorCopy( pm->ps->origin, up );
	up[2] += stepSize;

	pm->trace( &trace, up, pm->mins, pm->maxs, up, pm->ps->clientNum, pm->tracemask, qtrue );

	if ( trace.allsolid ) 
	{
		if ( pm->debugLevel ) 
		{
			Com_WPrintf("%i:bend can't step\n", c_pmove);
		}
		return 0.0f;		
	}

	VectorCopy( up, pm->ps->origin );

	PM_SlideMove( gravity );

	VectorSubtract( pm->ps->origin, up, diff );
	distance = VectorLength( diff );

	VectorCopy (pm->ps->origin, down);
	down[2] -= stepSize;
	pm->trace (&trace, pm->ps->origin, pm->mins, pm->maxs, down, pm->ps->clientNum, pm->tracemask, qtrue );

	if ( !trace.allsolid ) 
	{
		VectorCopy (trace.endpos, pm->ps->origin);
	}

	if ( trace.fraction < 1.0f ) 
	{
		PM_ClipVelocity( pm->ps->velocity, trace.plane.normal, pm->ps->velocity, OVERCLIP );
	}

	return distance;
}

void PM_StepSlideMove( qboolean gravity ) 
{
	vec3_t		start_o, start_v;
	float		try1Distance, try2Distance, simpleTryDistance;
	vec3_t		try1_o, try1_v;
	vec3_t		simpleTry1_o, simpleTry1_v;
	vec3_t		simpleDiff;


	VectorCopy( pm->ps->origin, start_o );
	VectorCopy( pm->ps->velocity, start_v );


	if ( PM_SlideMove( gravity ) == 0 ) 
	{
		return;		
	}


	VectorCopy( pm->ps->origin, simpleTry1_o );
	VectorCopy( pm->ps->velocity, simpleTry1_v );

	VectorSubtract( simpleTry1_o, start_o, simpleDiff );
	simpleTryDistance = VectorLength( simpleDiff );


	VectorCopy( start_o, pm->ps->origin );
	VectorCopy( start_v, pm->ps->velocity );


	try1Distance = PM_TryStepSlideMove( gravity, STEPSIZE );


	VectorCopy( pm->ps->origin, try1_o );
	VectorCopy( pm->ps->velocity, try1_v );


	VectorCopy( start_o, pm->ps->origin );
	VectorCopy( start_v, pm->ps->velocity );


	try2Distance = PM_TryStepSlideMove( gravity, STEPSIZE / 2.0f );


	if ( try1Distance > try2Distance )
	{
		VectorCopy( try1_o, pm->ps->origin );
		VectorCopy( try1_v, pm->ps->velocity );
	}

	if ( ( try1Distance < 0.001f ) && ( try2Distance < 0.001f ) && ( simpleTryDistance > 0.0f ) )
	{
		VectorCopy( simpleTry1_o, pm->ps->origin );
		VectorCopy( simpleTry1_v, pm->ps->velocity );
	}

	pm->stepped = qtrue;		

	if ( pm->debugLevel ) 
	{
		Com_Printf("%i:stepped\n", c_pmove);
	}
}

void PM_Friction
   (
   void
   )

   {
	vec3_t	vec;
	float	   *vel;
	float	   speed;
   float	   newspeed;
   float	   control;
	float	   drop;

	vel = pm->ps->velocity;

	VectorCopy( vel, vec );
	if ( pml.walking )
      {
		vec[ 2 ] = 0.0f;
	   }

	speed = VectorLength( vec );
	if ( speed < 1.0f )
      {
		vel[ 0 ] = 0;
		vel[ 1 ] = 0;

		return;
	   }

	drop = 0;

	if ( pml.walking )
      {
		if ( !( pm->ps->pm_flags & PMF_TIME_KNOCKBACK ) )
         {
			control = ( speed < (float)pm->ps->pm_stopspeed ) ? (float)pm->ps->pm_stopspeed : speed;
         if ( pml.groundTrace.surfaceFlags & SURF_SLICK )
            {
   			drop += control * pm_slipperyfriction * pml.frametime;
            }
         else
            {
   			drop += control * (float)pm->ps->pm_friction * pml.frametime;
            }
		   }
	   }

	if ( pm->waterlevel )
      {
      if ( pm->watertype & CONTENTS_SLIME )
         {
   		drop += speed * (float)pm->ps->pm_waterfriction * 5.0f * pm->waterlevel * pml.frametime;
         }
      else
         {
   		drop += speed * (float)pm->ps->pm_waterfriction * (float)pm->waterlevel * pml.frametime;
         }
	   }

	newspeed = speed - drop;
	if ( newspeed < 0.0f )
      {
		newspeed = 0;
	   }

	newspeed /= speed;

	vel[0] = vel[ 0 ] * newspeed;
	vel[1] = vel[ 1 ] * newspeed;
	vel[2] = vel[ 2 ] * newspeed;
   }


void PM_Accelerate
   (
   const vec3_t wishdir,
   float wishspeed,
   float accel
   )

   {
	int	i;
	float	addspeed;
   float accelspeed;
   float currentspeed;

	currentspeed = DotProduct( pm->ps->velocity, wishdir );
	addspeed = wishspeed - currentspeed;
	if ( addspeed <= 0.0f )
      {
		return;
	   }

	accelspeed = accel * pml.frametime * wishspeed;
	if ( accelspeed > addspeed )
      {
		accelspeed = addspeed;
	   }

	for( i = 0; i < 3; i++ )
      {
		pm->ps->velocity[ i ] += accelspeed * wishdir[ i ];
	   }
   }



float PM_CmdScale
   (
   const usercmd_t *cmd
   )

   {
	int	max;
	float	total;
	float	scale;

	max = abs( cmd->forwardmove );
	if ( abs( cmd->rightmove ) > max )
      {
		max = abs( cmd->rightmove );
	   }

	if ( abs( cmd->upmove ) > max )
      {
		max = abs( cmd->upmove );
	   }

	if ( !max )
      {
		return 0;
	   }

	if ( ( pm->ps->pm_flags & PMF_DUCKED ) && ( cmd->upmove >= 0 ) )
	{
		total = sqrt( ( cmd->forwardmove * cmd->forwardmove ) + ( cmd->rightmove * cmd->rightmove ) + ( -127 * -127 ) );
	}
	else
	{
		total = sqrt( ( cmd->forwardmove * cmd->forwardmove ) + ( cmd->rightmove * cmd->rightmove ) + ( cmd->upmove * cmd->upmove ) );
	}

	scale = (float)pm->ps->speed * (float)max / ( 127.0f * total );

	return scale;
   }


qboolean PM_CheckJump( void ) {

	if(pm->ps->pm_flags & PMF_DUCKED)
		return qfalse;


	if ( pm->cmd.upmove < 10 ) {
		return qfalse;
	}

	if ( pm->ps->pm_flags & PMF_JUMP_HELD ) {
		return qfalse;
	}

	pml.groundPlane = qfalse;		
 	pml.walking = qfalse;
	pm->ps->pm_flags |= PMF_JUMP_HELD;

	pm->ps->groundEntityNum = ENTITYNUM_NONE;
	pm->ps->velocity[2] = pm->ps->jumpvelocity;

	return qtrue;
}

qboolean	PM_CheckWaterJump
   (
   void
   )

   {
	vec3_t	spot;
	int		cont;

	if ( pm->ps->pm_time )
      {
		return qfalse;
	   }

	if ( ( pm->waterlevel < 2 ) )
      {
		return qfalse;
	   }
	if ( !( pm->watertype & CONTENTS_SLIME ) && ( pm->waterlevel == 3 ) )
      {
		return qfalse;
	   }

   VectorMA( pm->ps->origin, 80.0f, pml.flat_forward, spot );
	spot[ 2 ] += pm->ps->viewheight - 16.0f;
	cont = pm->pointcontents( spot, pm->ps->clientNum );
	if ( !( cont & pm->tracemask ) )
      {
		return qfalse;
	   }

	spot[ 2 ] += 48.0f;
	cont = pm->pointcontents( spot, pm->ps->clientNum );
	if ( cont )
      {
		return qfalse;
	   }

	VectorScale( pml.flat_forward, 150, pm->ps->velocity );
	pm->ps->velocity[ 2 ] = 600;

	pm->ps->pm_flags |= PMF_TIME_WATERJUMP;
	pm->ps->pm_time = 2000;

	return qtrue;
   }



void PM_WaterJumpMove
   (
   void
   )

   {
	PM_StepSlideMove( !( pm->ps->pm_flags & PMF_NO_GRAVITY ) );

	pm->ps->velocity[ 2 ] -= (float)pm->ps->gravity * pml.frametime;
	if ( pm->ps->velocity[ 2 ] < 0.0f )
      {
		pm->ps->pm_flags &= ~PMF_ALL_TIMES;
		pm->ps->pm_time = 0;
	   }
   }

#define SLIME_SINK_SPEED -10.0f
void PM_WaterMove
   (
   void
   )

   {
	int		i;
	vec3_t	wishvel;
	float	   wishspeed;
	vec3_t	wishdir;
	float	   scale;

	if ( PM_CheckWaterJump() )
      {
		PM_WaterJumpMove();
		return;
   	}

   if ( pm->watertype & CONTENTS_SLIME )
      {
      if ( pm->ps->velocity[ 2 ] < SLIME_SINK_SPEED )
         {
         pm->ps->velocity[ 2 ] = SLIME_SINK_SPEED;
         }
      }

	PM_Friction();

	scale = PM_CmdScale( &pm->cmd );

	if ( !scale )
      {
		wishvel[ 0 ] = 0;
		wishvel[ 1 ] = 0;
      if ( pm->watertype & CONTENTS_SLIME )
         {
	      wishvel[ 2 ] = SLIME_SINK_SPEED;		
         }
      else
         {
		   wishvel[ 2 ] = -60;		
         }
	   }
   else
      {
		for( i = 0; i < 3; i++ )
         {
			wishvel[ i ] = ( scale * pml.flat_forward[ i ] * pm->cmd.forwardmove ) - ( scale * pml.flat_left[ i ] * pm->cmd.rightmove );
         }

		wishvel[ 2 ] += scale * pm->cmd.upmove;
	   }
   if ( ( pm->watertype & CONTENTS_SLIME ) && ( pm->waterlevel > 2 ) && ( wishvel[ 2 ] < 0.0f ) )
      {
      wishvel[ 2 ] = 0;
      }

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( wishdir );

	if ( wishspeed > ( pm->ps->speed * pm_swimScale ) )
      {
		wishspeed = pm->ps->speed * pm_swimScale;
	   }

	PM_Accelerate( wishdir, wishspeed, pm->ps->pm_wateraccelerate );

	PM_SlideMove( qfalse );
   }

void PM_StuckJumpMove
   (
   void
   )

   {
	PM_StepSlideMove( !( pm->ps->pm_flags & PMF_NO_GRAVITY ) );

	pm->ps->velocity[ 2 ] -= pm->ps->gravity * pml.frametime;
	if ( pm->ps->velocity[ 2 ] < -48.0f )
      {
		pm->ps->pm_flags &= ~PMF_ALL_TIMES;
		pm->ps->pm_time = 0;
	   }
   }

void PM_FlyMove( void ) {
	float	   speed;
   float    drop;
   float    friction;
   float    control;
   float    newspeed;
	int		i;
	vec3_t	wishvel;
	float		fmove;
   float    smove;
	vec3_t	wishdir;
	float		wishspeed;
	float		scale;


	speed = VectorLength( pm->ps->velocity );
	if ( speed < 1.0f )
	   {
		VectorCopy( vec3_origin, pm->ps->velocity );
	   }
	else
	   {
		drop = 0;

		friction = pm->ps->pm_friction * 1.5f;

		control = speed < pm->ps->pm_stopspeed ? pm->ps->pm_stopspeed : speed;
		drop += control * friction * pml.frametime;

		newspeed = speed - drop;
		if ( newspeed < 0.0f )
         {
			newspeed = 0;
         }
		newspeed /= speed;

		VectorScale( pm->ps->velocity, newspeed, pm->ps->velocity );
	   }

	scale = PM_CmdScale( &pm->cmd );

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.rightmove;
   pm->ps->pm_runtime = 0;

	for( i = 0; i < 3; i++ )
      {
		wishvel[ i ] = ( pml.flat_forward[ i ] * fmove ) - ( pml.flat_left[ i ] * smove );
      }

	wishvel[ 2 ] += pm->cmd.upmove;

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( wishdir );
	wishspeed *= scale;

	PM_Accelerate( wishdir, wishspeed, pm->ps->pm_accelerate );

	PM_StepSlideMove( qtrue );
}

qboolean	PM_CheckStuckJump
   (
   void
   )

   {
   vec3_t   diff;

	if ( pm->ps->pm_time )
      {
		return qfalse;
	   }

	if ( pm->waterlevel > 1 )
      {
		return qfalse;
	   }

   VectorSubtract( pm->ps->origin, pml.previous_origin, diff );
   if ( VectorLength( diff ) )
      {
      return qfalse;
      }
   if ( VectorLength( pm->ps->velocity ) < 100.0f )
      {
      return qfalse;
      }


	return qtrue;
   }

#define TERMINAL_VELOCITY 1200.0f

void PM_CheckTerminalVelocity
   (
   void
   )
   {
   float oldspeed;
   float speed;

   oldspeed = -pml.previous_velocity[ 2 ];

   speed = -pm->ps->velocity[ 2 ];

	if ( speed <= 0.0f )
      {
		return;
	   }

	if ( ( oldspeed <= TERMINAL_VELOCITY ) && ( speed > TERMINAL_VELOCITY ) )
      {
		pm->pmoveEvent = EV_TERMINAL_VELOCITY;
   	}
   }

void PM_AirMove
   (
   void
   )

   {
	vec3_t		wishvel;
	float		   fmove;
   float       smove;
	vec3_t		wishdir;
	float		   wishspeed;
	float		   scale;
	usercmd_t	cmd;

	vec3_t		original_start;
	vec3_t		original_end;
	float			old_speed;
	vec3_t original_diff;
	float original_dist;
	vec3_t new_diff;
	float new_dist;
	float new_speed;
	float max_new_speed;


	VectorCopy( pm->ps->origin, original_start );
	VectorMA( pm->ps->origin, pml.frametime, pm->ps->velocity, original_end );
	old_speed = VectorLength( pm->ps->velocity );


	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.rightmove;
   pm->ps->pm_runtime = 0;

	cmd = pm->cmd;
	scale = PM_CmdScale( &cmd );

   wishvel[ 0 ] = ( pml.flat_forward[ 0 ] * fmove ) - ( pml.flat_left[ 0 ] * smove );
   wishvel[ 1 ] = ( pml.flat_forward[ 1 ] * fmove ) - ( pml.flat_left[ 1 ] * smove );
	wishvel[ 2 ] = 0.0f;

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( wishdir );
	wishspeed *= scale;

	PM_Accelerate( wishdir, wishspeed, pm->ps->pm_airaccelerate );

	if ( pml.groundPlane )
      {
		PM_ClipVelocity( pm->ps->velocity, pml.groundTrace.plane.normal, pm->ps->velocity, OVERCLIP );
	   }

   if ( !pml.walking && pml.groundPlane )
      {
      vec3_t vel;

		VectorCopy( pm->ps->velocity, vel );
		vel[ 2 ] -= pm->ps->gravity * pml.frametime;
		pm->ps->velocity[ 2 ] = ( pm->ps->velocity[ 2 ] + vel[ 2 ] ) * 0.5f;
      PM_SlideMove( qfalse );
      VectorCopy( vel, pm->ps->velocity );
      }
   else
      {
      PM_SlideMove( !( pm->ps->pm_flags & PMF_NO_GRAVITY ) );
      }
	
	if ( PM_CheckStuckJump() )
      {
		PM_StuckJumpMove();
   	}


	VectorSubtract( original_start, original_end, original_diff );
	original_dist = VectorLength( original_diff );


	VectorSubtract( original_start, pm->ps->origin, new_diff );
	new_dist = VectorLength( new_diff );

	if ( ( new_dist < original_dist ) && ( new_dist > 0 ) )
		{

		max_new_speed = old_speed * ( new_dist / original_dist );

		new_speed = VectorLength( pm->ps->velocity );

		if ( new_speed > max_new_speed )
			{
			VectorNormalize( pm->ps->velocity );
			VectorScale( pm->ps->velocity, max_new_speed, pm->ps->velocity );
			}
		}

   PM_CheckTerminalVelocity();
   }

void AddPlane
   (
   const vec3_t norm,
   vec3_t planes[ MAX_CLIP_PLANES ],
   int *numplanes
   )

   {
   int i;

   if ( *numplanes >= MAX_CLIP_PLANES )
      {
      return;
      }

   for( i = 0; i < *numplanes; i++ )
      {
      if ( VectorCompare( planes[ i ], norm ) )
         {
         return;
         }
      }

   VectorCopy( norm, planes[ *numplanes ] );
	( *numplanes )++;
   }

void PM_StepMove
   (
   void
   )

   {
   trace_t trace;
   vec3_t up;
   vec3_t down;
   vec3_t oldvelocity;
   vec3_t oldorigin;
   vec3_t velocity1;
   vec3_t origin1;

   VectorCopy( pm->ps->velocity, oldvelocity );
   VectorCopy( pm->ps->origin, oldorigin );

   if ( PM_SlideMove( !( pm->ps->pm_flags & PMF_NO_GRAVITY ) ) )
      {
      VectorCopy( pm->ps->velocity, velocity1 );
      VectorCopy( pm->ps->origin, origin1 );

      VectorCopy( oldvelocity, pm->ps->velocity );

      VectorCopy( oldorigin, up );
      up[ 2 ] += STEPSIZE;

      pm->trace( &trace, up, pm->mins, pm->maxs, up, pm->ps->clientNum, pm->tracemask, qtrue );
      VectorCopy( trace.endpos, pm->ps->origin );

      PM_SlideMove( !( pm->ps->pm_flags & PMF_NO_GRAVITY ) );

      VectorCopy( pm->ps->origin, down );
      down[ 2 ] = oldorigin[ 2 ];

      pm->trace( &trace, pm->ps->origin, pm->mins, pm->maxs, down, pm->ps->clientNum, pm->tracemask, qtrue );
      if ( trace.plane.normal[ 2 ] < MIN_WALK_NORMAL )
         {
         VectorCopy( velocity1, pm->ps->velocity );
         VectorCopy( origin1, pm->ps->origin );
         }
      else
         {
         VectorCopy( trace.endpos, pm->ps->origin );
         pm->stepped = qtrue;
         }
      }
   }


void PM_WalkMove
   (
   void
   )

   {
   int         i;
	vec3_t		wishvel;
	float		   fmove;
   float       smove;
	vec3_t		wishdir;
	float		   wishspeed;
	float		   scale;
	usercmd_t	cmd;
	float		   accelerate;
   float	      waterScale;

	if ( ( pm->waterlevel > 2 ) && ( DotProduct( pml.forward, pml.groundTrace.plane.normal ) > 0.0f ) )
      {
		PM_WaterMove();

		return;
	   }

	if ( pm->ps->instantJump )
		if ( PM_CheckJump () ) {
			pm->ps->jumped = qtrue;
			pm->ps->pm_time = 150;
			pm->ps->pm_flags |= PMF_TIME_JUMP_START;
			if ( pm->waterlevel > 1 ) {
				PM_WaterMove();
			} else {
				PM_AirMove();
			}
			return;
		}

	PM_Friction();

   fmove = pm->cmd.forwardmove;
   smove = pm->cmd.rightmove;

	cmd = pm->cmd;
	scale = PM_CmdScale( &cmd );

   if ( ( pm->cmd.buttons & BUTTON_RUN ) && fmove && !smove )
      {
      pm->ps->pm_runtime += pml.msec;
      }
   else
      {
      pm->ps->pm_runtime = 0;
      }


	PM_ClipVelocity (pml.flat_forward, pml.groundTrace.plane.normal, pml.flat_forward, OVERCLIP );
	PM_ClipVelocity (pml.flat_left, pml.groundTrace.plane.normal, pml.flat_left, OVERCLIP );

	VectorNormalize (pml.flat_forward);
	VectorNormalize (pml.flat_left);

	for( i = 0 ; i < 3 ; i++ )
      {
		wishvel[ i ] = pml.flat_forward[ i ] * fmove - pml.flat_left[ i ] * smove;
	   }

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( wishdir );
	wishspeed *= scale;

	if ( pm->waterlevel )
      {
		waterScale = pm->waterlevel / 3.0f;
		waterScale = 1.0f - ( 1.0f - pm_swimScale ) * waterScale;
		if ( wishspeed > ( pm->ps->speed * waterScale ) )
         {
			wishspeed = pm->ps->speed * waterScale;
		   }
	   }

	if ( ( pml.groundTrace.surfaceFlags & SURF_SLICK ) || ( pm->ps->pm_flags & PMF_TIME_KNOCKBACK ) )
      {
		accelerate = pm->ps->pm_airaccelerate;
	   }
   else
      {
		accelerate = pm->ps->pm_accelerate;
	   }

	PM_Accelerate( wishdir, wishspeed, accelerate );

	if ( ( pml.groundTrace.surfaceFlags & SURF_SLICK ) || ( pm->ps->pm_flags & PMF_TIME_KNOCKBACK ) )
      {
		pm->ps->velocity[ 2 ] -= pm->ps->gravity * pml.frametime;
	   }

	if ( !pm->ps->velocity[ 0 ] && !pm->ps->velocity[ 1 ] && !pm->ps->velocity[ 2 ] )
		return;

   PM_StepSlideMove ( !( pm->ps->pm_flags & PMF_NO_GRAVITY ) );
   }

void PM_DeadMove
   (
   void
   )

   {
	float	forward;

	if ( !pml.walking )
      {
		return;
	   }

	forward = VectorLength( pm->ps->velocity );
	forward -= 20.0f;
	if ( forward <= 0.0f )
      {
		VectorClear( pm->ps->velocity );
	   }
   else
      {
		VectorNormalize( pm->ps->velocity );
		VectorScale( pm->ps->velocity, forward, pm->ps->velocity );
	   }
   }


void PM_VehicleMove
   (
   void
   )

   {
	
  	vec3_t		wishvel;
	vec3_t		wishdir;
	float		   wishspeed;
	float		   scale;
	usercmd_t	cmd;
	float		   accelerate;
  

	cmd = pm->cmd;
	scale = PM_CmdScale( &cmd );

   pm->ps->pm_runtime = 0;

	wishvel[0] = pm->ps->velocity[0];
	wishvel[1] = pm->ps->velocity[1];
	wishvel[2] = pm->ps->velocity[2];

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( wishdir );
	wishspeed *= scale;
	
	accelerate = pm->ps->pm_accelerate;
	PM_Accelerate( wishdir, wishspeed, accelerate );

	if ( !pm->ps->velocity[ 0 ] && !pm->ps->velocity[ 1 ] )
		return;

   }



void PM_NoclipMove
   (
   void
   )

   {
	float	   speed;
   float    drop;
   float    friction;
   float    control;
   float    newspeed;
	int		i;
	vec3_t	wishvel;
	float		fmove;
   float    smove;
	vec3_t	wishdir;
	float		wishspeed;
	float		scale;

	pm->ps->viewheight = pm->ps->pm_defaultviewheight;


	speed = VectorLength( pm->ps->velocity );
	if ( speed < 1.0f )
	   {
		VectorCopy( vec3_origin, pm->ps->velocity );
	   }
	else
	   {
		drop = 0;

		friction = pm->ps->pm_friction * 1.5f;

		control = speed < pm->ps->pm_stopspeed ? pm->ps->pm_stopspeed : speed;
		drop += control * friction * pml.frametime;

		newspeed = speed - drop;
		if ( newspeed < 0.0f )
         {
			newspeed = 0;
         }
		newspeed /= speed;

		VectorScale( pm->ps->velocity, newspeed, pm->ps->velocity );
	   }

	scale = PM_CmdScale( &pm->cmd ) * 2.0f;

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.rightmove;
   pm->ps->pm_runtime = 0;

	for( i = 0; i < 3; i++ )
      {
		wishvel[ i ] = ( pml.flat_forward[ i ] * fmove ) - ( pml.flat_left[ i ] * smove );
      }

	wishvel[ 2 ] += pm->cmd.upmove;

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( wishdir );
	wishspeed *= scale;

	PM_Accelerate( wishdir, wishspeed, pm->ps->pm_accelerate );

	VectorMA( pm->ps->origin, pml.frametime, pm->ps->velocity, pm->ps->origin );
   }


void PM_CorrectAllSolid
   (
   void
   )

   {
	if ( pm->debugLevel )
      {
		Com_Printf( "%i:allsolid\n", c_pmove );
	   }

	pm->ps->groundEntityNum = ENTITYNUM_NONE;
	pml.groundPlane = qfalse;
	pml.walking = qfalse;
   }

void PM_GroundTrace
   (
   qboolean onlyTrace
   )

   {
	vec3_t   point;
	vec3_t	tmporg;
	trace_t  trace;

	point[ 0 ] = pm->ps->origin[ 0 ];
	point[ 1 ] = pm->ps->origin[ 1 ];
	point[ 2 ] = pm->ps->origin[ 2 ] - 9.0f; 

	pm->trace( &trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask, qtrue );

	if (!(trace.surfaceFlags & SURF_TERRAIN )) 
	{
		point[ 2 ] = pm->ps->origin[ 2 ] - 0.25f;
		pm->trace( &trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask, qtrue );
	}

	pml.groundTrace = trace;
	pm->ps->groundTrace = trace;

	if ( trace.allsolid )
      {
		tmporg[ 0 ] = pm->ps->origin[ 0 ];
		tmporg[ 1 ] = pm->ps->origin[ 1 ];
		tmporg[ 2 ] = pm->ps->origin[ 2 ] + 2.0f;

		pm->trace( &trace, tmporg, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask, qtrue );
		pml.groundTrace = trace;
		pm->ps->groundTrace = trace;

		if ( trace.allsolid )
			{
			PM_CorrectAllSolid();
			pm->ps->walking = pml.walking;
			pm->ps->groundPlane = pml.groundPlane;

			return;
			}
	   }

	if ( trace.fraction == 1.0f )
      {
		if ( pm->ps->groundEntityNum != ENTITYNUM_NONE )
         {
			if ( pm->debugLevel )
            {
				Com_Printf( "%i:lift\n", c_pmove );
			   }
		   }

		pm->ps->groundEntityNum = ENTITYNUM_NONE;
		pml.groundPlane = qfalse;
		pml.walking = qfalse;

      pm->ps->walking = pml.walking;
      pm->ps->groundPlane = pml.groundPlane;

      return;
	   }

	if ( trace.plane.normal[ 2 ] < MIN_WALK_NORMAL )
      {
      vec3_t oldvel;
      float d;

		if ( pm->debugLevel )
         {
			Com_Printf( "%i:steep\n", c_pmove );
		   }

      VectorCopy( pm->ps->velocity, oldvel );
      VectorSet( pm->ps->velocity, 0.0f, 0.0f, -1.0f / pml.frametime );
      PM_SlideMove( qfalse );

      d = VectorLength( pm->ps->velocity );
      VectorCopy( oldvel, pm->ps->velocity );
      if ( d > ( 0.1f / pml.frametime ) )
         {
		   pm->ps->groundEntityNum = ENTITYNUM_NONE;
		   pml.groundPlane = qtrue;
		   pml.walking = qfalse;

         pm->ps->walking = pml.walking;
         pm->ps->groundPlane = pml.groundPlane;

		   return;
         }
	   }

	if ( ( pm->ps->velocity[ 2 ] > 0.0f ) && ( DotProduct( pm->ps->velocity, trace.plane.normal ) > 10.0f ) )
      {
		if ( pm->debugLevel )
         {
			Com_Printf( "%i:kickoff\n", c_pmove );
		   }
		pm->ps->groundEntityNum = ENTITYNUM_NONE;
		pml.groundPlane = qfalse;
		pml.walking = qfalse;

      pm->ps->walking = pml.walking;
      pm->ps->groundPlane = pml.groundPlane;

		return;
	   }

	pml.groundPlane = qtrue;
	pml.walking = qtrue;

	if ( pm->ps->pm_flags & PMF_TIME_WATERJUMP )
	   {
		pm->ps->pm_flags &= ~( PMF_TIME_WATERJUMP );
		pm->ps->pm_time = 0;
	   }

	if ( pm->ps->pm_flags & PMF_TIME_STUCKJUMP )
	   {
		pm->ps->pm_flags &= ~( PMF_TIME_STUCKJUMP );
		pm->ps->pm_time = 0;
	   }

	if ( !onlyTrace && !( pml.groundTrace.surfaceFlags & SURF_SLICK ) )
		pm->ps->velocity[ 2 ] = 0;

	pm->ps->groundEntityNum = trace.entityNum;

	PM_AddTouchEnt( trace.entityNum );

   pm->ps->walking = pml.walking;
   pm->ps->groundPlane = pml.groundPlane;
   }


void PM_SetWaterLevel
   (
   void
   )

   {
	vec3_t  point;
	int	  cont;
	int	  sample1;
	int	  sample2;

	pm->waterlevel = 0;
	pm->watertype = 0;

	sample2 = pm->ps->viewheight - MINS_Z;
	sample1 = sample2 * 3 / 4;

   VectorCopy( pm->ps->origin, point );
   point[ 2 ] += MINS_Z + 1.0f;
	cont = pm->pointcontents( point, pm->ps->clientNum );
	if ( cont & MASK_WATER )
	   {
		pm->watertype = cont;
		pm->waterlevel = 1;

      point[ 2 ] = pm->ps->origin[ 2 ] + MINS_Z + sample1;
		cont = pm->pointcontents( point, 0 );
		if ( cont & MASK_WATER )
		   {
			pm->waterlevel = 2;
         point[ 2 ] = pm->ps->origin[ 2 ] + MINS_Z + sample2;
			cont = pm->pointcontents( point, 0 );
			if ( cont & MASK_WATER )
            {
				pm->waterlevel = 3;
            }
		   }
	   }
   }


void PM_CheckDuck
   (
   void
   )

   {
	pm->mins[ 0 ] = MINS_X;
	pm->mins[ 1 ] = MINS_Y;
	pm->mins[ 2 ] = MINS_Z;
	pm->maxs[ 0 ] = MAXS_X;
	pm->maxs[ 1 ] = MAXS_Y;
	pm->maxs[ 2 ] = MAXS_Z;

	pm->ps->viewheight = pm->ps->pm_defaultviewheight;
	
	if ( pm->ps->pm_type == PM_DEAD )
      {
		pm->maxs[ 2 ] = DEAD_MINS_Z;
		return;
	   }

	if ( pm->ps->pm_flags & PMF_DUCKED )
	   {
		
			pm->maxs[ 2 ] = CROUCH_MAXS_Z;
			pm->ps->viewheight = CROUCH_VIEWHEIGHT;
	   }
   }


void PM_CheckCrouchJump(void)
{
	if ( pm->ps->jumped && 
		 ( ( ( pm->ps->pm_flags & PMF_DUCKED ) && ( pm->ps->pm_flags & PMF_TIME_JUMP_START ) ) || 
		   ( !( pm->ps->pm_flags & PMF_TIME_JUMP_START ) && ( pm->ps->pm_flags & PMF_JUMP_HELD ) ) ) )
	{
 		if(pm->ps->crouchjumpset == qfalse)
		{
			pm->ps->pm_flags |= PMF_TIME_CROUCH_JUMP;
			pm->ps->crouchjumpset = qtrue;
		}
	}

	if(pm->ps->crouchjumpset == qtrue)
	{
		pm->ps->viewheight = pm->ps->pm_defaultviewheight;	
		pm->maxs[ 2 ] = MAXS_Z;
	}

	if(pm->ps->groundPlane)
	{
		pm->ps->crouchjumpset = qfalse;
	}
}


void PM_CrouchJumpMove(void)
{
	PM_StepSlideMove( !( pm->ps->pm_flags & PMF_NO_GRAVITY ) );
	pm->ps->velocity[2] += pm->ps->crouchjumpvelocity;

	pm->ps->pm_flags &= ~PMF_TIME_CROUCH_JUMP;
}


qboolean PM_ShouldCrashLand( void )
{

	
	if ( pm->ps->groundEntityNum != ENTITYNUM_NONE )
	{
		float delta;


		delta = pm->ps->velocity[ 2 ] - pml.previous_velocity[ 2 ];

		if ( delta < 0.0f || pml.previous_velocity[ 2 ] > -400 )
			return qfalse;

		if ( delta > 600.0f )
		{
			return qtrue;
		}
	}

	return qfalse;
}

void PM_CrashLand
   (
   void
   )

   {
	float delta;




	delta = pm->ps->velocity[ 2 ] - pml.previous_velocity[ 2 ];

	if ( delta < 0.0f || pml.previous_velocity[ 2 ] > -400 )
		return;


	if ( pm->waterlevel == 3 )
      {
		return;
	   }

	if ( pm->waterlevel == 2 )
      {
		delta *= 0.25f;
	   }

	if ( pm->waterlevel == 1 )
      {
		delta *= 0.5f;
	   }

	if ( delta < 1.0f )
      {
		return;
	   }

	pm->landed = qtrue;
	pm->landedVelocity = pml.previous_velocity[2];

	if ( !( pml.groundTrace.surfaceFlags & SURF_NODAMAGE ) )
	{
		if ( delta > 1500.0f )
		{
			pm->pmoveEvent = EV_FALL_FATAL;
		}
		else if ( delta > 1300.0f )
		{
			pm->pmoveEvent = EV_FALL_VERY_FAR;
		}
		else if ( delta > 1100.0f )
		{
			pm->pmoveEvent = EV_FALL_FAR;
		}
		else if ( delta > 900.0f )
		{
			pm->pmoveEvent = EV_FALL_MEDIUM;
		}
		else if ( delta > 750.0f )
		{
			pm->pmoveEvent = EV_FALL_SHORT;
		}
		else if ( delta > 600.0f )
		{
			pm->pmoveEvent = EV_FALL_VERY_SHORT;
		}
	}

   }

void PM_WaterEvents
   (
   void
   )

   {

	if ( ( pm->ps->pm_type == PM_SPECTATOR ) || ( pm->ps->pm_type == PM_SPECTATOR_FOLLOW ) )
		return;


	if ( !pml.previous_waterlevel && pm->waterlevel )
      {
		pm->pmoveEvent = EV_WATER_TOUCH;
	   }

	if ( pml.previous_waterlevel && ! pm->waterlevel )
      {
		pm->pmoveEvent = EV_WATER_LEAVE;
	   }

	if ( ( pml.previous_waterlevel != 3 ) && ( pm->waterlevel == 3 ) )
      {
		pm->pmoveEvent = EV_WATER_UNDER;
	   }

	if ( ( pml.previous_waterlevel == 3 ) && ( pm->waterlevel != 3 ) )
      {
		pm->pmoveEvent = EV_WATER_CLEAR;
	   }
   }

void PM_DropTimers
   (
   void
   )

   {
	if ( pm->ps->pm_time )
      {
		if ( pml.msec >= pm->ps->pm_time )
         {
			pm->ps->pm_flags &= ~PMF_ALL_TIMES;
			pm->ps->pm_time = 0;
		   }
      else
         {
			pm->ps->pm_time -= pml.msec;
		   }
	   }
   }



void PM_UpdateLeanIn( playerState_t* ps, const usercmd_t* cmd )
{
	vec3_t	viewOrigin;
	vec3_t	leanTraceMins;
	vec3_t	leanTraceMaxs;
	vec3_t	leftVector;
	vec3_t	leanTraceEnd;
	vec3_t	playerViewAngles;
	trace_t leanTrace;

	float leanDelta = ps->leanDelta;

	if( cmd->lean > 0 )
	{
		leanDelta += ((( float ) pml.msec / ( float ) pm_leaninspeed) * pm_leanmaxdelta);
		if( leanDelta > pm_leanmaxdelta )
			leanDelta = pm_leanmaxdelta;
	}
	else
	{
		leanDelta -= ((( float ) pml.msec / ( float ) pm_leaninspeed) * pm_leanmaxdelta);
		if( leanDelta < -pm_leanmaxdelta )
			leanDelta = -pm_leanmaxdelta;
	}


	VectorCopy( ps->origin, viewOrigin );
	viewOrigin[2] += ps->viewheight;

	VectorCopy( ps->viewangles, playerViewAngles );
	playerViewAngles[ROLL] = 0;

	AngleVectors( ps->viewangles, NULL, leftVector, NULL );
	VectorMA( viewOrigin, leanDelta, leftVector, leanTraceEnd );
	
	
	VectorSet( leanTraceMins, -5, -5, -4 );
	VectorSet( leanTraceMaxs, 5, 5, 4 );

	if( pm != 0)
	{
		pm->trace(&leanTrace, viewOrigin, leanTraceMins, leanTraceMaxs, 
						leanTraceEnd, ps->clientNum, MASK_PLAYERSOLID, qfalse);

		leanDelta *= leanTrace.fraction;
	}

	ps->leanDelta = leanDelta;
}


void PM_UpdateLeanOut( playerState_t* ps )
{
	float leanDelta = ps->leanDelta;

	if( leanDelta > 0 )
	{
		leanDelta -= (( (float) pml.msec / (float) pm_leanoutspeed) * pm_leanmaxdelta);
		if( leanDelta < 0 )
			leanDelta = 0;
	}
	else if ( leanDelta < 0 )
	{
		leanDelta += (((float)pml.msec / ( float ) pm_leanoutspeed) * pm_leanmaxdelta);
		if( leanDelta > 0)
			leanDelta = 0;
	}

	ps->leanDelta = leanDelta;
}


void PM_UpdateLeanView( playerState_t* ps, const usercmd_t* cmd )
{
	if(	cmd->lean == 0 )
	{
		PM_UpdateLeanOut(ps);
	}
	else 
	{
		PM_UpdateLeanIn(ps, cmd);
	}
}


void PM_UpdateViewAngles
   (
   playerState_t *ps,
   const usercmd_t *cmd
   )

   {
	short	temp;
	int	i;

	if ( ps->pm_flags & PMF_FROZEN )
      {
		return;
	   }


	for( i = 0; i < 3; i++ )
      {
		temp = cmd->angles[ i ] + ps->delta_angles[ i ];
		if ( i == PITCH )
         {
			if ( temp > 16000 )
            {
				ps->delta_angles[ i ] = 16000 - cmd->angles[ i ];
				temp = 16000;
			   }
         else if ( temp < -16000 )
            {
				ps->delta_angles[ i ] = -16000 - cmd->angles[ i ];
				temp = -16000;
			   }
		   }

		ps->viewangles[ i ] = SHORT2ANGLE( temp );
	   }


	PM_UpdateLeanView( ps, cmd );
   }


void Pmove_GroundTrace
   (
   pmove_t *pmove
   )

   {
   memset (&pml, 0, sizeof(pml));
   pml.msec = 1;
	pml.frametime = 0.001f;
	pm = pmove;
	if ( !( pm->ps->pm_flags & PMF_NO_MOVE ) )
	{
		PM_CheckDuck();
		PM_CheckCrouchJump();
	}

   PM_GroundTrace( qtrue );
   }


void PmoveSingle (pmove_t *pmove)
   {
   vec3_t tempVec;
   qboolean walking;
   vec3_t temp;

	pm = pmove;

	c_pmove++;

	pm->numtouch = 0;
	pm->watertype = 0;
	pm->waterlevel = 0;
	pm->landed = qfalse;

	if(pm->ps->groundPlane)
	{
		pm->ps->jumped = qfalse;
	}

	if ( pm->ps->stats[STAT_HEALTH] <= 0 )
      {
		pm->tracemask &= ~CONTENTS_BODY;	
	   }


	memset (&pml, 0, sizeof(pml));

	pml.msec = pmove->cmd.serverTime - pm->ps->commandTime;
	if ( pml.msec < 1 )
      {
		pml.msec = 1;
      }
      else if ( pml.msec > 200 )
      {
		pml.msec = 200;
	   }

	pm->ps->commandTime = pmove->cmd.serverTime;

	VectorCopy( pm->ps->origin, pml.previous_origin );

	VectorCopy( pm->ps->velocity, pml.previous_velocity );
	VectorCopy( pm->ps->velocity, temp );

	pml.frametime = pml.msec * 0.001f;

	if (!pm->ps->in_vehicle )
		{
		PM_UpdateViewAngles( pm->ps, &pm->cmd );

		AngleVectors( pm->ps->viewangles, pml.forward, pml.left, pml.up );
	   VectorClear( tempVec );
	   tempVec[ YAW ] = pm->ps->viewangles[ YAW ];
		AngleVectors( tempVec, pml.flat_forward, pml.flat_left, pml.flat_up );
		}

	if ( pm->ps->in_vehicle )
		{
		PM_VehicleMove();
		return;
		}

	if ( pm->cmd.upmove < 10 ) {
		pm->ps->pm_flags &= ~PMF_JUMP_HELD;
	}

	if ( pm->ps->pm_type == PM_DEAD )
      {
		pm->cmd.forwardmove = 0;
		pm->cmd.rightmove = 0;
		pm->cmd.upmove = 0;
		pm->cmd.lean = 0;
	   }

	if ( pm->ps->pm_type == PM_NOCLIP )
      {
		PM_NoclipMove ();
		PM_DropTimers ();
 		return;
	   }

	if ( ( pm->ps->pm_flags & PMF_FROZEN ) || ( pm->ps->pm_flags & PMF_NO_MOVE ) )
      {
		return;		
	   }

	PM_CheckDuck();

	PM_CheckCrouchJump();

	PM_SetWaterLevel();
	pml.previous_waterlevel = pmove->waterlevel;

	PM_GroundTrace( qfalse );

	if ( pm->ps->pm_type == PM_DEAD )
      {
		PM_DeadMove();
	   }

	PM_DropTimers();

	if ( pm->ps->pm_flags & PMF_FLIGHT )
		PM_FlyMove();
   else if ( pm->ps->pm_flags & PMF_TIME_WATERJUMP )
      {
		PM_WaterJumpMove();
	   }
   else if ( pml.walking )
      {
		PM_WalkMove();
	   }
   else if ( pm->waterlevel > 1 )
      {
		PM_WaterMove();
	   }
   else if ( pm->ps->pm_flags & PMF_TIME_STUCKJUMP )
      {
		PM_StuckJumpMove();
	   }
   else if( pm->ps->pm_flags & PMF_TIME_CROUCH_JUMP)
   {
	   PM_CrouchJumpMove();
   }
   else
      {
		if ( !pm->ps->in_vehicle )
			PM_AirMove();
	   }

   walking = pml.walking;

	PM_GroundTrace( qfalse );
	PM_SetWaterLevel();

   if ( !pml.walking && ( walking || ( ( pml.previous_velocity[ 2 ] >= 0.0f ) && ( pm->ps->velocity[ 2 ] <= 0.0f ) ) ) )
      {
	   vec3_t   point;
	   trace_t  trace;

	   point[ 0 ] = pm->ps->origin[ 0 ];
	   point[ 1 ] = pm->ps->origin[ 1 ];
	   point[ 2 ] = pm->ps->origin[ 2 ] - 0.5f;

	   pm->trace( &trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask, qtrue );
      if ( ( trace.fraction < 1.0f ) && ( !trace.allsolid ) )
         {
   		VectorCopy( trace.endpos, pm->ps->origin );

   		pm->stepped = qtrue;

         PM_GroundTrace( qfalse );
   	   PM_SetWaterLevel();
         }
      }

	PM_WaterEvents();


	
	SnapVector( pm->ps->velocity );


	if ( !pm->ps->strafeJumpingAllowed && ( pm->ps->groundEntityNum != ENTITYNUM_NONE ) && ( pm->ps->groundEntityNum == ENTITYNUM_WORLD ) )
	{
		float speed;
		vec3_t xyVelocity;



		VectorCopy( pm->ps->velocity, xyVelocity );


		speed = VectorLength( xyVelocity );

		if ( speed > pm->ps->speed )
		{

			VectorNormalize( xyVelocity );
			VectorScale( xyVelocity, pm->ps->speed, pm->ps->velocity );


			
		}
	}

	if ( PM_ShouldCrashLand() )
	{
		if ( pm->debugLevel )
		{
			Com_Printf( "%i:Land\n", c_pmove );
		}

		PM_CrashLand();
	}

   }

void Pmove (pmove_t *pmove) {
	int			finalTime;

	finalTime = pmove->cmd.serverTime;

	if ( finalTime < pmove->ps->commandTime ) {
		return;	
	}

	if ( finalTime > ( pmove->ps->commandTime + 1000 ) ) {
		pmove->ps->commandTime = finalTime - 1000;
	}

	while ( pmove->ps->commandTime != finalTime ) {
		int		msec;

		msec = finalTime - pmove->ps->commandTime;

		if ( msec > 50 ) {
			msec = 50;
		}
		pmove->cmd.serverTime = pmove->ps->commandTime + msec;
		PmoveSingle( pmove );
	}

}
