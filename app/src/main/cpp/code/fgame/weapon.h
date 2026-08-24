/*
===========================================================================
Copyright (C) 2025 the OpenMoHAA team

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


#pragma once

#include "g_local.h"
#include "item.h"
#include "ammo.h"
#include "../corepp/queue.h"
#include "sentient.h"
#include "../fgame/scriptmaster.h"

extern Event EV_Weapon_Shoot;
extern Event EV_Weapon_DonePutaway; 
extern Event EV_Weapon_SetAmmoClipSize;
extern Event EV_Weapon_SetAmmoInClip;
extern Event EV_Weapon_FireDelay;
extern Event EV_Weapon_SetProjectile;
extern Event EV_Weapon_SetBulletDamage;
extern Event EV_Weapon_SetBulletLarge;
extern Event EV_Weapon_SetTracerSpeed;
extern Event EV_Weapon_SetBulletKnockback;
extern Event EV_Weapon_SetBulletThroughWood;
extern Event EV_Weapon_SetBulletThroughMetal;
extern Event EV_Weapon_SetBulletCount;
extern Event EV_Weapon_SetBulletSpread;
extern Event EV_Weapon_SetTracerFrequency;
extern Event EV_Weapon_GiveStartingAmmo;
extern Event EV_Weapon_SetMeansOfDeath;

// past the end of the list so they could not collide with a retail .sav; they are gone
typedef enum {
    FT_NONE,               
    FT_BULLET,             
    FT_PROJECTILE,         
    FT_MELEE,              
    FT_SPECIAL_PROJECTILE, 
    FT_EXPLOSION,
    FT_TRIGGER_PROJECTILE,
    FT_CONTROL_PROJECTILE,
    FT_CONTROL_ZOOM,
} firetype_t;

typedef enum {
    ACCURACY_STOPPED,
    ACCURACY_CHANGE,
    ACCURACY_WALK,
    ACCURACY_RUN,
    ACCURACY_CROUCH,
    MAX_ACCURACYTYPES
} weaponaccuracy_t;

typedef enum {
    ZOOM_NORMAL_FOV = 0,
    ZOOM_STAGE_1,
    ZOOM_STAGE_2,
    NUM_ZOOM_STAGES
} ZoomStage;

typedef enum {
    WEAPONBONE_BARREL_TAG
} weaponbones_t;

typedef enum {
    WEAPON_ANIMATING,  
    WEAPON_READY,     
    WEAPON_FIRING,    
    WEAPON_LOWERING,  
    WEAPON_RAISING,   
    WEAPON_HOLSTERED, 
    WEAPON_RELOADING, 
    WEAPON_CHANGING,  
    WEAPON_SWITCHINGMODE
} weaponstate_t;

#define INITIALIZE_WEAPONMODE_VAR(var, value)       \
    {                                               \
        int _ii;                                    \
        for (_ii = 0; _ii < MAX_FIREMODES; _ii++) { \
            var[_ii] = value;                       \
        }                                           \
    }

class Player;

class WeaponProperty : public Class
{
public:
    WeaponProperty() : _property(), _stringValue(), _floatValue(0.f) {}
    WeaponProperty(const str& property) : _property(property) {}
    ~WeaponProperty() {}

    const str& getStringValue() { return _stringValue; }
    float      getFloatValue() { return _floatValue; }
    const str& getProperty() { return _property; } 

    void setStringValue(const str& value) { _stringValue = value; }
    void setFloatValue(float value) { _floatValue = value; }

private:
    str   _property;
    str   _stringValue;
    float _floatValue;
};

class WeaponProperties : public Class
{
public:
    WeaponProperties();
    ~WeaponProperties() {}

    WeaponProperty getWeaponProperty(const str& name);
    void           addWeaponProperty(const str& weaponProperty);

private:
    Container<WeaponProperty> _weaponProperties;
};

class Weapon : public Item
{
protected:
    qboolean   attached;            
    float      nextweaponsoundtime; 
    str        current_attachToTag; 
    str        left_attachToTag;         
    str        right_attachToTag;        
    str        dual_attachToTag;         
    str        leftholster_attachToTag;  
    str        rightholster_attachToTag; 
    str        dualholster_attachToTag;  
    float      lastScale;                
    Vector     lastAngles;          
    qboolean   lastValid;           
    qboolean   auto_putaway;        
    qboolean   use_no_ammo;         
    qboolean   crosshair;           
    qboolean   torsoaim;             
    qboolean   special_move;         
    qboolean   targetidleflag;       
    qboolean   thirdperson;          
    bool       useActorAiming;       
    int        chx;                  
    int        chy;                  
    Vector     realvieworg;          
    Vector     endbulletspread[MAX_FIREMODES];  
    float      shootingMoveSpeedModifier[MAX_FIREMODES];  
    float      reticuletime;         
    int        aimtype;              
    float      chargetime;           
    qboolean   usesameclip;          
    qboolean   targetidle;           
    float      _projectileDamage;    

protected:
    float         maxrange;                
    float         minrange;                
    str           viewmodel;               
    weaponstate_t weaponstate;             
    int           rank;                    
    int           order;                   
    SentientPtr   last_owner;              
    float         last_owner_trigger_time; 
    qboolean      notdroppable;            
    int           aimanim;                 
    int           aimframe;                
    Vector        leftHolsterAngles;
    Vector        rightHolsterAngles;
    Vector        dualHolsterAngles;
    float         holsterScale;            
    float         _weildedScale;
    float         next_noise_time;         
    float         next_noammo_time;        


    str            ammo_type[MAX_FIREMODES];          
    int            ammorequired[MAX_FIREMODES];       
    int            startammo[MAX_FIREMODES];          
    str            projectileModel[MAX_FIREMODES];    
    float          bulletdamage[MAX_FIREMODES];       
    float          bulletcount[MAX_FIREMODES];        
    float          bulletrange[MAX_FIREMODES];        
    float          bulletknockback[MAX_FIREMODES];    
    float          projectilespeed[MAX_FIREMODES];    
    Vector         bulletspread[MAX_FIREMODES];       
    firetype_t     firetype[MAX_FIREMODES];           
    int            ammo_clip_size[MAX_FIREMODES];     
    int            ammo_in_clip[MAX_FIREMODES];       
    float          max_charge_time[MAX_FIREMODES];    
    meansOfDeath_t meansofdeath[MAX_FIREMODES];       
    qboolean       loopfire[MAX_FIREMODES];           
    // ActionLevelIncrement().  Note: retail itself never calls ActionLevelIncrement() --
    int            action_level_increment[MAX_FIREMODES]; 
    str            worldhitspawn[MAX_FIREMODES];      
    qboolean       quiet;  

    float          fire_timer[MAX_FIREMODES];         
    float          next_fire_time[MAX_FIREMODES];     
    qboolean       switchmode;                        
    firemode_t     curmode;                           
    firemode_t     maxmode;                           
    int            shootingSkin;                      
    int            targetingSkin;                     
    bool           _noAmmoMode[MAX_FIREMODES];        
    qboolean       burstmode[MAX_FIREMODES];          
    float          _burstModeDelay[MAX_FIREMODES];
    int            burstcount;                        
    int            burstcountmax;                     
    int            _chargedModels[MAX_FIREMODES];
    float          _spreadTime[MAX_FIREMODES];        
    int            _spreadAnims[MAX_FIREMODES];       
    float          startfiretime;                     
    int            _ammoBoost[MAX_FIREMODES];         
    Vector         _fireOffset[MAX_FIREMODES];        
    float          _powerRating;                      
    int            _regenAmount[MAX_FIREMODES];       
    float          _regenTime[MAX_FIREMODES];         
    bool           _regenOnlyWhenIdle[MAX_FIREMODES]; 
    float          _nextRegenTime[MAX_FIREMODES];     
    float          _projectileSpeed;                  
    bool           _allowAutoSwitch;                  
    float          _nextSwitchTime;                   

    float          accuracy[MAX_FIREMODES][MAX_ACCURACYTYPES];

    float          _viewShakeMagnitude[MAX_FIREMODES]; 
    float          _viewShakeDuration[MAX_FIREMODES];  
    Vector         _viewMinShake[MAX_FIREMODES];       
    Vector         _viewMaxShake[MAX_FIREMODES];       
    bool           _viewShakeOverride[MAX_FIREMODES];  
    float          _maxViewShakeChange;                
    Vector         viewShake;                          

    bool           _arcProjectile;                    
    float          _lowArcRange;                      
    bool           _playMissSound;                    

    float          _meleeWidth[MAX_FIREMODES];
    float          _meleeHeight[MAX_FIREMODES];
    float          _meleeLength[MAX_FIREMODES];

    bool           _noDelay[MAX_FIREMODES];           
    bool           _canInterruptFiringState;          
    bool           _autoReload;                       

    int            _fullAmmoSkin;
    firemode_t     _fullAmmoMode;

    bool           _controllingProjectile;
    bool           _controllingProjectileHidden;      
    str            _controlEmitterName;               
    str            _controlSoundName;                 

    float          zoomfov;                           
    float          _lastZoomFov;
    float          startzoom;                         
    float          endzoom;                           
    float          zoomtime;                          
    float          startzoomtime;
    qboolean       zoomed;
    ZoomStage      _zoomStage;

    qboolean       donefiring;

    int   autoAimTargetSelectionAngle; 
    int   autoAimLockonAngle;          

    Container<EntityPtr> meleeVictims;
    float charge_fraction;             
    qboolean   putaway;         
    inline qboolean GetPutaway(void) { return putaway; };   
    inline void     SetPutAway(qboolean p) { putaway = p; };  
    firemode_t firemodeindex;
    weaponhand_t hand;
        float defaultMoveSpeedModifier;  

public:
    SafePtr<Entity> aim_target;

protected:
    friend class Player;
    friend class WeaponDualWield;
    friend class Sentient;

    void SetMaxRangeEvent(Event *ev);
    void SetMinRangeEvent(Event *ev);

    void         DetachGun(void);
    void         AttachGun(weaponhand_t hand, qboolean holstering = qfalse);
    void         PickupWeapon(Event *ev);
    void         DoneRaising(Event *ev);
    void         DonePutaway(Event *ev); 
    void         DoneFiring(Event *ev);
    void         FinishedFiring(Event *ev); 
    qboolean     IsDoneFiring();             
    void         ForceReload(Event *ev);    
    void         Idle(Event *ev);
    void         DoneReloading(Event *ev);
    void         DoneReloadingBurst(Event *ev);
    void         SetAimAnim(Event *ev);
    virtual void Shoot(Event *ev);
    void         ModeSet(Event *ev); 
    void         SetFireType(Event *ev);
    void         SetProjectile(Event *ev);
    void         SetBulletDamage(Event *ev);
    void         SetBulletRange(Event *ev);
    void         SetBulletKnockback(Event *ev);
    void         SetBulletCount(Event *ev);
    void         SetBulletSpread(Event *ev);
    void         SetAutoPutaway(Event *ev);
    void         SetRange(Event *ev);
    void         SetUseNoAmmo(Event *ev);
    void         AttachToTag(Event *ev);                 
    void         SetSpecialMove(Event *ev);             
    void         SetSecondaryAmmo(const char *type, int amount, int startamount);  
    Vector       GetViewEndPoint(void);                 
    void         LeftAttachToTag(Event *ev);         
    void         RightAttachToTag(Event *ev);        
    void         DualAttachToTag(Event *ev);         
    void         LeftHolsterAttachToTag(Event *ev);  
    void         RightHolsterAttachToTag(Event *ev); 
    void         DualHolsterAttachToTag(Event *ev);  
    void         SetLeftHolsterAngles(Event *ev);    
    void         SetRightHolsterAngles(Event *ev);   
    void         SetDualHolsterAngles(Event *ev);    
    void         SetHolsterScale(Event *ev);
    void         setWeildedScale(Event *ev); 
    void         SetWorldHitSpawn(Event *ev);

public:
    CLASS_PROTOTYPE(Weapon);

    Weapon();
    Weapon(const char *file);
    ~Weapon();

    int  GetRank(void);
    int  GetOrder(void);

    virtual void ProcessTargetedEntity(EntityPtr entity);  




    void  SetRank(int order, int rank);
    float GetMaxRange(void);
    float GetMinRange(void);



    void             SetMaxRange(float val);
    void             SetMinRange(float val);
    void             ForceIdle(void);
    void             SetAnim(const str &animName, Event *endevent = NULL, bodypart_t bodypart = legs);
    void             AddEffectsAnims(void);
    void             playAnim(const str& animName, bool animatingFlag = true);

    inline float     GetFireTime(firemode_t mode) { return next_fire_time[mode]; }
    inline qboolean  GetSwitchMode() { return switchmode; }

    void       SetAmmoRequired(Event *ev);
    void       SetStartAmmo(Event *ev);
    int        GetStartAmmo(firemode_t mode);
    int        GetMaxAmmo(firemode_t mode);
    str        GetAmmoType(firemode_t mode);
    firetype_t GetFireType(firemode_t mode);
    void       SetAmmoType(Event *ev);
    void       SetAmmoAmount(int amount, firemode_t mode);
    void       UseAmmo(int amount, firemode_t mode);
    void       SetAmmoClipSize(Event *ev);
    void       SetAmmoInClip(Event *ev);
    void       SetModels(const char *world, const char *view);
    void       SetOwner(Sentient *ent) override;
    void       SetMaxChargeTime(Event *ev);
    int        AmmoAvailable(firemode_t mode);
    qboolean   UnlimitedAmmo(firemode_t mode);
    qboolean   HasAmmo(firemode_t mode, int numShots = 1);
    qboolean   HasAmmoInClip(firemode_t mode, int numShots = 1);
    int        GetClipSize(firemode_t mode);

    qboolean CheckReload(void);
    bool     canReload(void);
    qboolean HasFullClip(void);

    virtual qboolean ReadyToFire(firemode_t mode, qboolean playsound = qtrue);

    void         PutAway(void);
    qboolean     Drop(void) override;
    void         Fire(firemode_t mode);
    void         Charge(firemode_t mode);
    void         ReleaseFire(firemode_t mode, float chargetime);
    void         ClientFireDone(void);
    qboolean     Removable(void) override;
    qboolean     Pickupable(Entity *other) override;
    virtual void DetachFromOwner(void);
    virtual void AttachToOwner(weaponhand_t hand);
    virtual void Uninitialize(void);
    void         WeaponSound(Event *ev);

    void GetActorMuzzlePosition(
        Vector *position, Vector *forward = NULL, Vector *right = NULL, Vector *up = NULL, const char *tagname = NULL
    );

    qboolean      AutoChange(void);
    virtual int   getStat(int statNum) { return 0; }
    int           ClipAmmo(firemode_t mode);
    int           GetRequiredAmmo(firemode_t mode);
    int           getAmmoInClip(firemode_t mode);
    qboolean      IsDroppable(void);

    void          ForceState(weaponstate_t state);

    void         NotDroppableEvent(Event *ev);
    void         AutoAim(Event *ev);
    void         Crosshair(Event *ev);
    void         SetQuiet(Event *ev);
    void         SetLoopFire(Event *ev);

    void         SetFireTimer(Event *ev);         
    void         noAmmoMode(Event *ev);           
    bool         hasNoAmmoMode(firemode_t mode);  
    void         SetPowerRating(Event *ev);       
    void         SetPowerRating(float damage);     
    float        GetPowerRating();                 
    void         SetRegenAmmo(Event *ev);         
    void         SetRegenOnlyWhenIdle(Event *ev); 
    virtual void Think(void) override;            
    void         SetProjectileSpeed(Event *ev);   
    void         SetProjectileSpeed(float speed) { _projectileSpeed = speed; }
    float        GetProjectileSpeed(void) const { return _projectileSpeed; }
    inline float        GetBulletDamage(firemode_t idx = FIRE_MODE1) { return bulletdamage[idx]; }  
    inline float        GetProjectileDamage() { return _projectileDamage; }
    inline void         SetCurrentAttachToTag(const str& s) { current_attachToTag = s; }
    inline qboolean     GetUseNoAmmo(void) { return use_no_ammo; }
    inline void         setAttached(bool flag) { attached = flag; }
    float        GetBulletSpreadX(firemode_t idx) const { return bulletspread[idx].x; }
    float        GetRange(firemode_t idx);
    void         SetBurstMode(Event *ev);         
    void         setBurstModeDelay(Event *ev);    
    void         setChargedModels(Event *ev);     
    void         SetSwitchMode(Event *ev);        
    void         SetMaxModes(Event *ev);          
    void         UseSameClip(Event *ev);           
    void         updateViewShake(void);           
    void         startViewShake(void);
    void         reduceViewShake(Event *ev);      
    void         clearViewShake(Event *ev);
    Vector       getViewShake(void) const { return viewShake; }

    void         SetArcProjectile(Event *ev);     
    void         SetLowArcRange(Event *ev);       
    void         SetPlayMissSound(Event *ev);     
    bool         shouldArcProjectile(void) const { return _arcProjectile; }
    float        GetLowArcRange(void) const { return _lowArcRange; }
    bool         shouldPlayMissSound(void) const { return _playMissSound; }

    void         setMeleeParms(Event *ev);        

    void         setNoDelay(Event *ev);           
    bool         isModeNoDelay(firemode_t mode);  
    void         setCanInterruptFiringState(Event *ev); 
    void         cacheStrings(void) override;
    void         setAutoReload(Event *ev);        
    void         ChangeIdle(Event *ev);           
    void         StartFiring(Event *ev);          
    void         SetAccuracy(Event *ev);          

    void         setFullAmmoSkin(Event *ev);      

    void         setControlParms(Event *ev);          
    void         setProjectileControlHidden(Event *ev); 
    void         toggleProjectileControl(void);
    bool         getControllingProjectile(void);  

    //      It is dead code in retail too; `zoomed` can never become true.
    void         SetZoomFOV(Event *ev);   
    void         SetStartZoom(Event *ev); 
    void         SetEndZoom(Event *ev);   
    void         SetZoomTime(Event *ev);  
    void         setZoomStage(Event *ev); 
    void         Zoom(Event *ev);         
    void         endZoom(Event *ev);      
    void         rezoom(Event *ev);       
    void         IncrementZoom(Event *ev);
    void         changeZoomStage(float firstZoom, float secondZoom);
    weaponhand_t GetHand(void) const { return hand; }
    void         SetShootingSkin(Event *ev);      
    void         SetTargetingSkin(Event *ev);     

    void         SetActionLevelIncrement(Event *ev); 
    int          ActionLevelIncrement(firemode_t mode);

    void         setAllowAutoSwitch(Event *ev);   
    bool         shouldAutoSwitch(firemode_t mode);
    void         autoSwitch(void);
    str          getWeaponByPriority(int priority);
    int          getWeaponPriority(void);
    void         setNextSwitchTime(float time);


    virtual void AttachToHolster(weaponhand_t hand);
    str          GetCurrentAttachToTag(void);
    str          GetLeftHolsterTag(void);
    str          GetRightHolsterTag(void);
    str          GetDualHolsterTag(void);

    void           SetMeansOfDeath(Event *ev);
    meansOfDeath_t GetMeansOfDeath(firemode_t mode);
    void           SetAimTarget(Entity *);
    void           WorldHitSpawn(firemode_t mode, const Vector &org, const Vector &angles, float life); 
    void           MakeNoise(Event *ev);


    void Archive(Archiver& arc) override;

    Vector getSpread(void);
    void   applySpread(Vector *forward, Vector *right, Vector *up);

    inline firemode_t GetCurMode() { return curmode; }  
    void ClearMeleeVictims() { meleeVictims.ClearObjectList(); }  
    void AdvancedMeleeAttack(const char *tag1, const char *tag2, bool critical = false);  
    void       DoneAnimating(Event *ev);


    float  RespawnTime(void) override;
    void   CheckForTargetedEntity(void);
    void   PassToAnimate(Event *ev) override;
    Vector getViewShake(void);
    void   pauseRegen(Event *ev);
    void   processGameplayData(Event *ev) override;

    qboolean HasInvAmmo(firemode_t mode);
    void     TorsoAim(Event *ev);
    void     giveAmmoBoost(Event *ev);
    void     ArchivePersistantData(Archiver &arc) override;






public:
    void setAmmoBoost( Event *ev ); 
    int getAmmoBoost( firemode_t mode ); 
    void GetMuzzlePosition( Vector *position, Vector *forward = NULL, Vector *right = NULL, Vector *up = NULL ); 
    void GiveStartingAmmo( Event *ev ); 
    void SetAnim( const str &animName, const Event &endevent, bodypart_t part = legs ); 
    qboolean CheckReload( firemode_t mode ); 
    qboolean ForceReload( ); 
    void ProcessWeaponCommandsEvent(Event *ev); 
    void SetBulletSpread( float spreadX , float spreadY ); 
    void SetHand( Event *ev ); 
    void SetViewModel( Event *ev ); 
    void DrawBowStrain( Event *ev ); 
    void AltDrawBowStrain( Event *ev ); 
    void SetReticuleTime( Event *ev ); 
    void SetAimType( Event *ev ); 
    void TargetIdle( Event *ev ); 
    void TargetIdleThink( Event *ev ); 
    void SwitchMode( ); 
    void DoneSwitchToMiddle( Event *ev ); 
    void DoneSwitching( Event *ev ); 
    void SetCHOffset(int chx, int chy); 
    void SetRealViewOrigin( const Vector &rv ); 
    void SetThirdPerson( qboolean tp ); 
    void endZoom( void ); 
    void setMoveSpeedModifier( Event *ev ); 
    void setShootingMoveSpeedModifier( Event *ev ); 
    float getMoveSpeedModifier( void ); 
    void UseActorAiming( Event *ev ); 
    void startViewShake( Event *ev ); 
    void setViewShakeInfo( Event *ev ); 
    void setAdvancedViewShakeInfo( Event *ev ); 
    void SetProjectileDamage( Event *ev ); 
    void SetProjectileDamage( float damage );  

    void setControllingProjectile( Event *ev ); 
    void setSpreadAnimData( Event *ev ); 
    void setMaxViewShakeChange( Event *ev ); 
    void setFireOffset( Event *ev ); 
    void forceReload( Event *ev ); 
    void setNextSwitchTime( Event *ev ); 
};

inline void Weapon::ArchivePersistantData(Archiver &arc)
{
    int i;

    for (i = 0; i < MAX_FIREMODES; i++) {
        arc.ArchiveInteger(&ammo_in_clip[i]);
    }
}

//   * `quiet` is a SCALAR in retail (SDK weapon.h:784), not one flag per firemode.
//   * several fields retail archives are never initialised in its own constructor
// MOHAA members this port has but retail never archived -- m_iAnimSlot,
inline void Weapon::Archive(Archiver& arc)
{
    int             i, j, k;
    SafePtr<Entity> discardPtr;

    Item::Archive(arc);

    arc.ArchiveBoolean(&attached);
    arc.ArchiveFloat(&nextweaponsoundtime);
    arc.ArchiveString(&current_attachToTag);

    arc.ArchiveString(&left_attachToTag);
    arc.ArchiveString(&right_attachToTag);
    arc.ArchiveString(&dual_attachToTag);
    arc.ArchiveString(&leftholster_attachToTag);
    arc.ArchiveString(&rightholster_attachToTag);
    arc.ArchiveString(&dualholster_attachToTag);

    arc.ArchiveFloat(&lastScale);
    arc.ArchiveVector(&lastAngles);
    arc.ArchiveBoolean(&lastValid);
    arc.ArchiveBoolean(&auto_putaway);
    arc.ArchiveBoolean(&use_no_ammo);
    arc.ArchiveBoolean(&crosshair);
    arc.ArchiveBoolean(&torsoaim);
    arc.ArchiveBoolean(&special_move);
    arc.ArchiveSafePointer(&aim_target);

    ArchiveEnum(curmode, firemode_t);
    ArchiveEnum(maxmode, firemode_t);

    arc.ArchiveBoolean(&switchmode);
    arc.ArchiveBoolean(&targetidleflag);

    arc.ArchiveInteger(&chx);
    arc.ArchiveInteger(&chy);

    arc.ArchiveVector(&realvieworg);
    arc.ArchiveBoolean(&thirdperson);

    arc.ArchiveBool(&useActorAiming);

    arc.ArchiveFloat(&_powerRating);
    arc.ArchiveFloat(&_projectileDamage);
    arc.ArchiveFloat(&_projectileSpeed);

    arc.ArchiveBool(&_arcProjectile); 
    arc.ArchiveFloat(&_lowArcRange);
    arc.ArchiveBool(&_playMissSound); 

    {
        int numEntries;
        if (arc.Saving()) {
            numEntries = meleeVictims.NumObjects();
            arc.ArchiveInteger(&numEntries);

            EntityPtr eptr;
            for (i = 1; i <= numEntries; i++) {
                eptr = meleeVictims.ObjectAt(i);
                arc.ArchiveSafePointer(&eptr);
            }
        } else {
            EntityPtr   eptr;
            EntityPtr  *eptrptr;
            arc.ArchiveInteger(&numEntries);

            if (numEntries < 0 || numEntries > 4096) {
                arc.FileError("Weapon: meleeVictims count %d is out of range", numEntries);
                return;
            }

            meleeVictims.Resize(numEntries);

            for (i = 1; i <= numEntries; i++) {
                meleeVictims.AddObject(eptr);
                eptrptr = &meleeVictims.ObjectAt(i);
                arc.ArchiveSafePointer(eptrptr);
            }
        }
    }

    arc.ArchiveFloat(&maxrange);
    arc.ArchiveFloat(&minrange);
    arc.ArchiveString(&viewmodel);

    if (arc.Loading()) {
        gi.setviewmodel(edict, viewmodel.c_str());
    }

    ArchiveEnum(weaponstate, weaponstate_t);

    arc.ArchiveInteger(&rank);
    arc.ArchiveInteger(&order);
    arc.ArchiveSafePointer(&last_owner);
    arc.ArchiveFloat(&last_owner_trigger_time);
    arc.ArchiveBoolean(&notdroppable);
    arc.ArchiveInteger(&aimanim);
    arc.ArchiveInteger(&aimframe);

    arc.ArchiveVector(&leftHolsterAngles);
    arc.ArchiveVector(&rightHolsterAngles);
    arc.ArchiveVector(&dualHolsterAngles);

    arc.ArchiveFloat(&holsterScale);
    arc.ArchiveFloat(&_weildedScale);

    arc.ArchiveBoolean(&quiet);

    arc.ArchiveFloat(&next_noise_time);
    arc.ArchiveFloat(&next_noammo_time);

    arc.ArchiveInteger(&burstcount);
    arc.ArchiveInteger(&burstcountmax);


    for (i = 0; i < MAX_FIREMODES; i++) {
        arc.ArchiveString(&ammo_type[i]);
        arc.ArchiveInteger(&ammorequired[i]);
        arc.ArchiveInteger(&startammo[i]);
        arc.ArchiveInteger(&_ammoBoost[i]);
        arc.ArchiveString(&projectileModel[i]);
        arc.ArchiveFloat(&bulletdamage[i]);
        arc.ArchiveFloat(&bulletcount[i]);
        arc.ArchiveFloat(&bulletrange[i]);
        arc.ArchiveFloat(&bulletknockback[i]);
        arc.ArchiveFloat(&projectilespeed[i]);
        arc.ArchiveVector(&bulletspread[i]);
        arc.ArchiveVector(&endbulletspread[i]);

        ArchiveEnum(firetype[i], firetype_t);

        arc.ArchiveInteger(&ammo_clip_size[i]);
        arc.ArchiveInteger(&ammo_in_clip[i]);
        arc.ArchiveFloat(&max_charge_time[i]);

        ArchiveEnum(meansofdeath[i], meansOfDeath_t);

        arc.ArchiveBoolean(&loopfire[i]);
        arc.ArchiveInteger(&action_level_increment[i]);
        arc.ArchiveString(&worldhitspawn[i]);
        arc.ArchiveFloat(&next_fire_time[i]);
        arc.ArchiveFloat(&fire_timer[i]);

        for (j = 0; j < MAX_ACCURACYTYPES; j++) {
            for (k = 0; k < 5; k++) {
                arc.ArchiveFloat(&accuracy[i][j]);
            }
        }

        arc.ArchiveBoolean(&burstmode[i]);
        arc.ArchiveFloat(&_burstModeDelay[i]);
        arc.ArchiveFloat(&shootingMoveSpeedModifier[i]);
        arc.ArchiveFloat(&_viewShakeMagnitude[i]);
        arc.ArchiveFloat(&_viewShakeDuration[i]);
        arc.ArchiveVector(&_viewMinShake[i]);
        arc.ArchiveVector(&_viewMaxShake[i]);
        arc.ArchiveBool(&_viewShakeOverride[i]); 
        arc.ArchiveBool(&_noAmmoMode[i]);        
        arc.ArchiveBool(&_noDelay[i]);           
        arc.ArchiveInteger(&_chargedModels[i]);
        arc.ArchiveInteger(&_spreadAnims[i]);
        arc.ArchiveFloat(&_spreadTime[i]);
        arc.ArchiveFloat(&_meleeWidth[i]);
        arc.ArchiveFloat(&_meleeHeight[i]);
        arc.ArchiveFloat(&_meleeLength[i]);
        arc.ArchiveVector(&_fireOffset[i]);
        arc.ArchiveInteger(&_regenAmount[i]);
        arc.ArchiveFloat(&_regenTime[i]);
        arc.ArchiveBool(&_regenOnlyWhenIdle[i]); 
        arc.ArchiveFloat(&_nextRegenTime[i]);
    }

    arc.ArchiveFloat(&_maxViewShakeChange);
    arc.ArchiveFloat(&startfiretime);
    arc.ArchiveInteger(&autoAimTargetSelectionAngle);  
    arc.ArchiveInteger(&autoAimLockonAngle);           
    arc.ArchiveFloat(&charge_fraction);
    arc.ArchiveBoolean(&putaway);

    ArchiveEnum(firemodeindex, firemode_t);


    ArchiveEnum(hand, weaponhand_t);

    arc.ArchiveFloat(&reticuletime);

    arc.ArchiveFloat(&zoomfov);
    arc.ArchiveFloat(&_lastZoomFov);
    arc.ArchiveFloat(&startzoom);
    arc.ArchiveFloat(&endzoom);
    arc.ArchiveFloat(&zoomtime);
    arc.ArchiveFloat(&startzoomtime);

    arc.ArchiveInteger(&aimtype);
    arc.ArchiveBoolean(&usesameclip);  
    arc.ArchiveFloat(&chargetime);
    arc.ArchiveBoolean(&targetidle);
    arc.ArchiveBoolean(&donefiring);
    arc.ArchiveBoolean(&zoomed);

    arc.ArchiveInteger(&targetingSkin);
    arc.ArchiveInteger(&shootingSkin);
    arc.ArchiveInteger(&_fullAmmoSkin);

    ArchiveEnum(_fullAmmoMode, firemode_t);

    arc.ArchiveFloat(&defaultMoveSpeedModifier);  

    arc.ArchiveVector(&viewShake);

    arc.ArchiveBool(&_controllingProjectile);       
    arc.ArchiveBool(&_controllingProjectileHidden); 
    arc.ArchiveString(&_controlEmitterName);
    arc.ArchiveString(&_controlSoundName);
    arc.ArchiveBool(&_canInterruptFiringState); 

    ArchiveEnum(_zoomStage, ZoomStage);

    arc.ArchiveBool(&_autoReload);      
    arc.ArchiveBool(&_allowAutoSwitch); 
    arc.ArchiveFloat(&_nextSwitchTime);
}

typedef SafePtr<Weapon> WeaponPtr;

inline void Weapon::SetPowerRating( float damage )
{
	_powerRating = damage;
}

inline void Weapon::SetProjectileDamage( float damage )
{
	_projectileDamage = damage;
}

