//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/weaputils.h                                   $
// $Revision:: 43                                                             $
//   $Author:: Steven                                                         $
//     $Date:: 4/09/03 2:32p                                                  $
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).


#pragma once

#include "g_local.h"
#include "animate.h"
#include "beam.h"

#define P_BOUNCE_TOUCH    (1 << 0)
#define P_CHARGE_LIFE     (1 << 1)
#define P_CHARGE_SPEED    (1 << 2)
#define P_BOUNCE_ALL      (1 << 3) 
#define P_QUIET_EXPIRE    (1 << 4) 
#define P_NO_TOUCH_DAMAGE (1 << 5)
#define P_FUSE            (1 << 6)

extern Event EV_Projectile_Explode;
extern Event EV_Projectile_UpdateBeam;

extern Event EV_Actor_ProjectileClose;

class Weapon;
class Sentient;

class Projectile : public Entity
{
public:
    ~Projectile();  
public:
	CLASS_PROTOTYPE( Projectile );

	int owner;
	float speed;
	float minspeed;
	float bouncefactor;
	float damage;
	float knockback;
	float life;
	float dmlife;
	float minlife;
	float dlight_radius;
	float charge_fraction;

	SafePtr<Weapon> weap;
	Vector dlight_color;
	Vector addvelocity;
	meansOfDeath_t meansofdeath;
	class FuncBeam *m_beam;
	bool _scaleByCharge;				 
	float _minScaleFromCharge;			 
	float _maxScaleFromCharge;			 
	bool _scaleExplosion;				 
	int projFlags;
	str bouncesound;
	str impactmarkshader;
	str impactmarkorientation;
	float impactmarkradius;
	str explosionmodel;
	SafePtr<Entity> target;
	bool drunk;
	float drunkAngleModifier;
	bool addownervelocity;
	bool can_hit_owner;
	bool remove_when_stopped;
	bool m_bExplodeOnTouch;
	bool m_bHurtOwnerOnly;
	int m_iSmashThroughGlass;
	bool m_bArcToTarget;
	bool m_bDieInWater;

	bool _notShootable;


	float  startTime;
	bool   stick_on_touch;
	bool   ownercontrol;
	bool   _ownerControlLaser;
	bool   _ownerControlUsed;
	float  _controlTurnSpeed;
	bool   firstTimeOwnerControl;
	Vector prevPlayerAngles;
	bool   triggerdetonate;
	bool   _notifyActors;
	bool   _hitsProjectiles;
	float  _minOnGroundTime;
	bool   _onGround;
	float  _startOnGroundTime;
	bool   _damagedSomething;

	Projectile();
	void Archive( Archiver &arc ) override;
	virtual void Touch( Event *ev );
	virtual void Explode( Event *ev );
	virtual void DoDecal( void );

	void			SetAvelocity( Event *ev );
	void			SetAddVelocity( Event *ev );
	void			SetDLight( Event *ev );
	void			SetLife( Event *ev );
	void			SetMinLife( Event *ev );
	void			SetChargeLife( Event *ev );
	void			SetFuse( Event *ev ); 
	void			SetSpeed( Event *ev );
	void			SetMinSpeed( Event *ev );
	void			SetChargeSpeed( Event *ev );
	void			SetDamage( Event *ev );
	void			SetKnockback( Event *ev );
	void			SetProjectileDLight( Event *ev );
	void			SetMeansOfDeath( Event *ev );
	void			SetBounceTouch( Event *ev );
	void			SetBounceFactor( Event *ev );  
	void			SetBounceSound( Event *ev );
	void			SetImpactMarkShader( Event *ev );
	void			SetImpactMarkRadius( Event *ev );
	void			SetImpactMarkOrientation( Event *ev );
	void			SetExplosionModel( Event *ev );
	void			setNotShootable( Event *ev );
	void			SetQuietExpire( Event *ev );		 
	void			StickOnTouch( Event *ev );			 
	void			SetOwnerControl( Event *ev );		 
	void			setOwnerControlLaser( Event *ev );	 
	void			setControlTurnSpeed( Event *ev );	 
	void			TriggerDetonate( Event *ev );		 
	void			AngleThink( Event *ev );			 
	void			setNotifyActors( Event *ev );		 
	void			setHitsProjectiles( Event *ev );	 
	void			setMinOnGroundTime( Event *ev );	 
	void			NotifyActors( void );				 
	void			didDamage( void );					 
	void			UpdateBeam( Event *ev );
	void			BeamCommand( Event *ev );
	void			HeatSeek( Event *ev );
	void			Drunk( Event *ev );
	void			AddOwnerVelocity( Event *ev );
	float			ResolveMinimumDistance( Entity *potential_target, float currmin );
	float			AdjustAngle( float maxadjust, float currangle, float targetangle );
	void			SetCanHitOwner( Event *ev );
	void			ClearOwner( Event *ev );
	Entity *		getOwner( void );  
	void			RemoveWhenStopped( Event *ev );
	void			Stopped( Event *ev );
	void			Think() override;
	void			SetOwner(Entity* owner); 






public:
    void destroyed( Event *ev ); 
    void setScaleByCharge( Event *ev ); 
    void setScaleExplosion( Event *ev ); 
};


// were interleaved through the middle, and eleven retail members were simply not written.
// Sentient::Archive.  MOHAA members retail never archived are simply not written -- they
inline void Projectile::Archive
	(
	Archiver &arc
	)
{
	float           fzero = 0.0f;
	bool            bzero = false;
	Vector          vzero( 0, 0, 0 );

	Entity::Archive( arc );

	arc.ArchiveFloat( &fzero );				 
	arc.ArchiveInteger( &owner );
	arc.ArchiveFloat( &speed );
	arc.ArchiveFloat( &minspeed );
	arc.ArchiveFloat( &bouncefactor );
	arc.ArchiveFloat( &damage );
	arc.ArchiveFloat( &knockback );
	arc.ArchiveFloat( &life );
	arc.ArchiveFloat( &minlife );
	arc.ArchiveFloat( &dlight_radius );
	arc.ArchiveFloat( &charge_fraction );
	arc.ArchiveBool( &firstTimeOwnerControl );

	arc.ArchiveVector( &prevPlayerAngles );
	arc.ArchiveVector( &vzero );			 
	arc.ArchiveVector( &dlight_color );
	arc.ArchiveVector( &addvelocity );
	ArchiveEnum( meansofdeath, meansOfDeath_t );
	arc.ArchiveObjectPointer( ( Class ** )&m_beam );
	arc.ArchiveInteger( &projFlags );
	arc.ArchiveString( &bouncesound );
	arc.ArchiveString( &impactmarkshader );
	arc.ArchiveString( &impactmarkorientation );
	arc.ArchiveFloat( &impactmarkradius );
	arc.ArchiveString( &explosionmodel );

	arc.ArchiveSafePointer( &target );

	arc.ArchiveBool( &addownervelocity );
	arc.ArchiveBool( &drunk );
	arc.ArchiveBool( &can_hit_owner );
	arc.ArchiveBool( &remove_when_stopped );

	arc.ArchiveBool( &stick_on_touch );
	arc.ArchiveBool( &ownercontrol );
	arc.ArchiveBool( &_ownerControlLaser );
	arc.ArchiveBool( &_ownerControlUsed );
	arc.ArchiveFloat( &_controlTurnSpeed );
	arc.ArchiveBool( &triggerdetonate );

	arc.ArchiveFloat( &startTime );
	arc.ArchiveFloat( &drunkAngleModifier );

	arc.ArchiveBool( &_scaleByCharge );
	arc.ArchiveFloat( &_minScaleFromCharge );
	arc.ArchiveFloat( &_maxScaleFromCharge );
	arc.ArchiveBool( &_scaleExplosion );
	arc.ArchiveBool( &_notifyActors );
	arc.ArchiveBool( &_notShootable );
	arc.ArchiveBool( &_hitsProjectiles );

	arc.ArchiveFloat( &_minOnGroundTime );
	arc.ArchiveBool( &_onGround );
	arc.ArchiveFloat( &_startOnGroundTime );
	arc.ArchiveBool( &bzero );				 
	arc.ArchiveBool( &_damagedSomething );
}

class Explosion : public Projectile
{
	public:
	float		flash_r,
				flash_g,
				flash_b,
				flash_a,
				flash_minradius,
				flash_radius,
				flash_mintime,
				flash_time;

	int			flash_type;
	float		radius_damage;
	bool		hurtOwnerOnly;

	   CLASS_PROTOTYPE( Explosion );

	   float			radius;
	   qboolean			constant_damage;
	   qboolean			damage_every_frame;

	   Explosion();
	   void				SetRadius( Event *ev );
	   void				SetRadiusDamage( Event *ev );
	   void				SetConstantDamage( Event *ev );
	   void				SetDamageEveryFrame( Event *ev );
	   void				SetFlash( Event *ev );

	   void				DamageAgain( Event *ev );

	   void Archive( Archiver &arc ) override;
};

inline void Explosion::Archive
	(
	Archiver &arc
	)
{
	Projectile::Archive( arc );

	arc.ArchiveFloat( &flash_r );
	arc.ArchiveFloat( &flash_g );
	arc.ArchiveFloat( &flash_b );
	arc.ArchiveFloat( &flash_a );
	arc.ArchiveFloat( &flash_minradius );
	arc.ArchiveFloat( &flash_radius );
	arc.ArchiveFloat( &flash_mintime );
	arc.ArchiveFloat( &flash_time );
	arc.ArchiveInteger( &flash_type );
	arc.ArchiveFloat( &radius_damage );
}

qboolean MeleeAttack
   (
   const Vector &pos,
   const Vector &end,
   float damage,
   Entity *attacker,
	meansOfDeath_t means_of_death,
	float attack_width,
	float attack_min_height,
	float attack_max_height,
	float knockback = 0,
	qboolean hit_dead = true,
   Container<EntityPtr>*victimlist=NULL,
   Weapon *weapon = NULL,
   bool critical = false
   );

Projectile *ProjectileAttack
   (
   Vector         start,
   Vector         dir,
   Entity         *owner,
   str            projectileModel,
   float          speedfraction,
	float				real_speed = 0,
	Weapon			*weap = NULL
   );

void ExplosionAttack
(
	const Vector   &pos,
	Entity         *owner,
	const str      &projectileModel,
	Vector         dir = Vector( 0.0f, 0.0f, 0.0f ),
	Entity         *ignore = NULL,
	float          scale = 1.0f
);  

float BulletAttack(
    const Vector& start,
    const Vector& dir,
    const Vector& right,
    const Vector& up,
    float         range,
    float         damage,
    float         knockback,
    int           dflags,
    int           meansofdeath,
    const Vector& spread,
    int           count,
    Entity*       owner,
    Entity*       weap);

float RadiusDamage
(
	Entity   *inflictor,
	Entity   *attacker,
	float    damage,
	Entity   *ignore,
	int      mod,
	float    radius = 0,
	float    knockback = 0,
	qboolean constant_damage = false
);  

void StunAttack
(
	const Vector   &pos,
	Entity   *attacker,
	Entity   *inflictor,
	float    radius,
	float    time,
	Entity   *ignore
);

const char* G_LocationNumToDispString(int iLocation);
