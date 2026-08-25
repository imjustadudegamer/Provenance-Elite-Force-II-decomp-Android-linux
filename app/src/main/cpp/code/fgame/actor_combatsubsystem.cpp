
#include "actor.h"
#include "actor_combatsubsystem.h"
#include "actor_locomotion.h"
#include "actor_enemymanager.h"
#include "../qcommon/gameplaymanager.h"
#include "weapon.h"
#include "archive.h"
#include "g_phys.h"      
#include "gamecvars.h"   
#include "object.h"      
#include "scriptslave.h" 

// SDK actor_combatsubsystem.cpp:32-36 -- the default constructor is a deliberate hard
// error in retail.  Mac PPC has no symbol for it (never called, eliminated), so this is
CombatSubsystem::CombatSubsystem()
{
    gi.Error(ERR_FATAL, "CombatSubsystem::CombatSubsystem -- Default Constructor Called");
}

CombatSubsystem::CombatSubsystem(Actor *actor)
    : act(NULL)
{
    if (actor) {
        act = actor;
    } else {
        gi.Error(ERR_DROP, "MovementSubsystem::MovementSubsystem -- actor is NULL");
    }

    _init();
}

CombatSubsystem::~CombatSubsystem() {}

bool CombatSubsystem::HaveWeapon()
{
	if ( _activeWeapon.weapon )
		return true;
	
	return false;
}

str CombatSubsystem::GetActiveWeaponName()
{
	str name;

	if (_activeWeapon.weapon) {
		name = _activeWeapon.weapon->getName();
	}

	return name;
}

str CombatSubsystem::GetActiveWeaponArchetype()
{
	str atype;

	if (_activeWeapon.weapon) {
		atype = _activeWeapon.weapon->getArchetype();
	}

	return atype;
}

const str CombatSubsystem::GetAnimForMyWeapon(const str& property)
{
    if (!act) {
        return "";
    }

    GameplayManager *gpm = GameplayManager::getTheGameplayManager();

    if (!gpm || !gpm->hasObject(act->getArchetype())) {
        return "";
    }

    str objname = GetActiveWeaponArchetype();
    objname     = "Hold" + objname;

    if (gpm->hasProperty(objname, property)) {
        return gpm->getStringValue(objname, property);
    }

    return "";
}

float CombatSubsystem::GetDataForMyWeapon(const str& property)
{
    if (!act) {
        return 0.0f;
    }

    GameplayManager *gpm = GameplayManager::getTheGameplayManager();

    if (!gpm || !gpm->hasObject(act->getArchetype())) {
        return 0.0f;
    }

    str objname = GetActiveWeaponArchetype();
    objname     = "Hold" + objname;

    if (gpm->hasProperty(objname, property)) {
        return gpm->getFloatValue(objname, property);
    }

    return 0.0f;
}


void CombatSubsystem::GetGunPositionData(Vector *pos, Vector *forward, Vector *right, Vector *up)
{
    // @0x0006CB90 reads _activeWeapon.weapon (+20), not act->GetActiveWeapon(WEAPON_DUAL).
    Weapon *weapon = _activeWeapon.weapon;

    if (!weapon) {
        gi.Error(ERR_FATAL, "Actor has no activeweapon");
    }

    int tag_num = gi.Tag_NumForName(weapon->edict->s.modelindex, "tag_barrel");

    if (tag_num < 0) {
        gi.Error(ERR_FATAL, "Weapon has no tag_barrel");
    }

    weapon->GetActorMuzzlePosition(pos, forward, right, up);
}

void CombatSubsystem::FireWeapon()
{
    firemode_t mode = FIRE_MODE1;
    mode = WeaponModeNameToNum("primary");

    if (_activeWeapon.weapon) {
        _activeWeapon.weapon->Fire(mode);
    }
}

void CombatSubsystem::StopFireWeapon()
{
    if (_activeWeapon.weapon) {
        if (_activeWeapon.weapon->animate->HasAnim("endfire")) {
            _activeWeapon.weapon->SetAnim("endfire");
        } else {
            _activeWeapon.weapon->ForceIdle();
        }
    }
}

void CombatSubsystem::AimWeaponTag(const Vector& targetPos)
{
    Vector aimAngles;
    Vector gunPos, gunForward, gunRight, gunUp;

    Weapon *weapon = _activeWeapon.weapon;

    if (!weapon) {
        return;
    }

    weapon->setOrigin();
    weapon->setAngles();

    weapon->SetControllerAngles(WEAPONBONE_BARREL_TAG, vec_zero);

    GetGunPositionData(&gunPos, &gunForward, &gunRight, &gunUp);

    float gfToWorld[3][3];
    float worldToGf[3][3];

    gunForward.copyTo(gfToWorld[0]);
    gunRight.copyTo(gfToWorld[1]);
    gunUp.copyTo(gfToWorld[2]);

    TransposeMatrix(gfToWorld, worldToGf);

    Vector barrelToEnemyVector = targetPos - gunPos;
    vec3_t barrelToEnemyVec3_t;
    barrelToEnemyVector.copyTo(barrelToEnemyVec3_t);

    vec3_t barrelToEnemyTransformedVec3_t;
    MatrixTransformVector(barrelToEnemyVec3_t, worldToGf, barrelToEnemyTransformedVec3_t);

    Vector barrelToEnemyTransformed(barrelToEnemyTransformedVec3_t);
    barrelToEnemyTransformed.normalize();
    barrelToEnemyTransformed = barrelToEnemyTransformed.toAngles();
    barrelToEnemyTransformed.EulerNormalize();

    aimAngles = -barrelToEnemyTransformed;

    Vector aimUp, aimLeft, aimForward;
    float  spreadX, spreadY;

    aimAngles.AngleVectors(&aimForward, &aimLeft, &aimUp);
    spreadX = GetDataForMyWeapon("spreadx");
    spreadY = GetDataForMyWeapon("spready");

    if (weapon->GetFireType(FIRE_MODE1) == FT_PROJECTILE) {
        aimForward = (aimForward * weapon->GetRange(FIRE_MODE1)) + (aimLeft * G_CRandom(spreadX))
                   + (aimUp * G_CRandom(spreadY));
    }

    aimForward.normalize();

    aimAngles = aimForward.toAngles();

    weapon->SetControllerTag(WEAPONBONE_BARREL_TAG, gi.Tag_NumForName(weapon->edict->s.modelindex, "tag_barrel"));
    weapon->SetControllerAngles(WEAPONBONE_BARREL_TAG, aimAngles);
}

void CombatSubsystem::AimWeaponTag(Entity *target)
{
    if (!target) {
        return;
    }

    str    targetBone = target->getTargetPos();
    Vector targetPos  = target->centroid;

    if (targetBone.length()) {
        if (gi.Tag_NumForName(target->edict->s.modelindex, targetBone.c_str()) > 0) {
            target->GetTag(targetBone.c_str(), &targetPos, NULL, NULL, NULL);
        }
    }

    Vector newTargetPos = targetPos;

    Weapon *weapon = _activeWeapon.weapon;

    if (!weapon) {
        return;
    }

    if (weapon->GetFireType(FIRE_MODE1) == FT_PROJECTILE) {
        newTargetPos = GetLeadingTargetPos(weapon->GetProjectileSpeed(), targetPos, target);
    }

    AimWeaponTag(newTargetPos);
}

void CombatSubsystem::ClearAim()
{
    Weapon *weapon = _activeWeapon.weapon;

    if (!weapon) {
        return;
    }

    weapon->SetControllerTag(WEAPONBONE_BARREL_TAG, gi.Tag_NumForName(weapon->edict->s.modelindex, "tag_barrel"));
    weapon->SetControllerAngles(WEAPONBONE_BARREL_TAG, vec_zero);
}

bool CombatSubsystem::shouldArcProjectile()
{
    if (_activeWeapon.weapon) {
        return _activeWeapon.weapon->shouldArcProjectile();
    }

    return false;
}

float CombatSubsystem::GetLowArcRange()
{
    if (_activeWeapon.weapon) {
        return _activeWeapon.weapon->GetLowArcRange();
    }

    return 0.0f;
}

bool CombatSubsystem::GetProjectileLaunchAngles(
    Vector&       launchAngles,
    const Vector& launchPoint,
    const float   initialSpeed,
    const float   gravity,
    const bool    useHighTrajectory
) const
{
    Entity const *target = act ? (Entity *)act->enemyManager->GetCurrentEnemy() : NULL;

    if (target) {
        const Vector targetPoint(target->centroid);

        Trajectory projectileTrajectory(
            launchPoint, targetPoint, initialSpeed, gravity * -sv_currentGravity->value, useHighTrajectory
        );

        if (projectileTrajectory.GetTravelTime() > 0.0f) {
            launchAngles.setPitch(projectileTrajectory.GetLaunchAngle());

            Vector direction(targetPoint - launchPoint);
            direction.z = 0.0f;
            direction.normalize();

            launchAngles.setYaw(direction.toYaw());
            launchAngles.setRoll(0.0f);
            return true;
        }
    }

    return false;
}

void CombatSubsystem::Archive(Archiver& arc)
{
    _activeWeapon.Archive(arc);
    arc.ArchiveFloat(&_nextTimeTracedToTarget);
    arc.ArchiveFloat(&_traceInterval);
    arc.ArchiveBool(&_canShootTarget);

    arc.ArchiveFloat(&_yawDiff);
}

void CombatSubsystem::DoArchive(Archiver& arc, Actor *actor)
{
    Archive(arc);
    if (actor) {
        act = actor;
    } else {
        gi.Error(ERR_FATAL, "MovementSubsystem::DoArchive -- actor is NULL");
    }
}


bool CombatSubsystem::CanAttackTarget(Entity *target)
{
    Vector startPos;

    if (!target) {
        return false;
    }

    if (!act) {
        return false;
    }

    if (act->CurrentAnim(EF2ANIM_LEGS) < 0) {
        return false;
    }

    if (_activeWeapon.weapon && (FT_MELEE != _activeWeapon.weapon->GetFireType(FIRE_MODE1))) {
        GetGunPositionData(&startPos);
    } else {
        startPos = act->centroid;
    }

    return CanAttackTargetFrom(target, startPos);
}

bool CombatSubsystem::CanAttackTargetFrom(Entity *target, const Vector& startPos)
{
    if (!target) {
        return false;
    }

    if (_activeWeapon.weapon && !IsTargetInWeaponRange(target)) {
        return false;
    }

    Vector startToTarget = target->centroid - startPos;
    startToTarget.normalize();

    if (_activeWeapon.weapon && act->movementSubsystem) {
        float dot = DotProduct(startToTarget, act->movementSubsystem->getAnimDir());

        if (dot <= -0.10f) {
            return false;
        }
    }

    if (level.time < _nextTimeTracedToTarget) {
        return _canShootTarget;
    }

    _canShootTarget          = _traceHitTarget(target, startPos);
    _nextTimeTracedToTarget = level.time + _traceInterval + G_Random();

    return _canShootTarget;
}

bool CombatSubsystem::IsTargetInWeaponRange(Entity *target)
{
    if (!_activeWeapon.weapon || !act) {
        return false;
    }

    Vector distance = target->origin - act->origin;

    if (distance.length() <= _activeWeapon.weapon->GetMaxRange()) {
        return true;
    }

    return false;
}

bool CombatSubsystem::_traceHitTarget(Entity *target, const Vector& startPos)
{
    Entity *t;
    int     mask;
    trace_t trace;
    Vector  targetPos;
    str     targetBone;

    mask = MASK_SHOT;

    targetBone = target->getTargetPos();
    targetPos  = target->centroid;

    if (targetBone.length()) {
        if (gi.Tag_NumForName(target->edict->s.modelindex, targetBone) > 0) {
            target->GetTag(targetBone.c_str(), &targetPos, NULL, NULL, NULL);
        }
    }

    Vector attackDir      = targetPos - startPos;
    float  attackDirLength = attackDir.length();

    float uivk_cdist = attackDirLength;

    attackDir.normalize();
    attackDir *= attackDirLength + 32.0f;
    attackDir += startPos;
    targetPos = attackDir;

    trace = G_FullTrace(startPos, vec_zero, vec_zero, targetPos, act, mask, false,
                        "CombatSubsystem::_traceHitTarget");

    if (gi.cvar("qa_probe", "0", 0)->integer) {
        gi.Printf("[EVENT:F7:aimtrace] e=%d target=%d start=(%.1f,%.1f,%.1f) end=(%.1f,%.1f,%.1f) startsolid=%d hitent=%d mask=0x%08x cdist=%.1f\n",
                  act->entnum, target->entnum,
                  startPos[0], startPos[1], startPos[2],
                  targetPos[0], targetPos[1], targetPos[2],
                  trace.startsolid ? 1 : 0,
                  trace.ent ? trace.ent->s.number : -1,
                  mask,
                  uivk_cdist);
    }

    if (trace.startsolid) {
        if (trace.ent == target->edict) {
            return true;
        }
        return false;
    }

    if (!trace.ent) {
        return false;
    }

    if (trace.ent == target->edict) {
        return true;
    }

    t = trace.ent->entity;
    if (act->enemyManager->IsValidEnemy(t)) {
        act->enemyManager->SetCurrentEnemy(trace.ent->entity);
        return false;
    }

    if (t->isSubclassOf(Object) || t->isSubclassOf(ScriptModel)) {
        trace = G_Trace(Vector(trace.endpos), vec_zero, vec_zero, target->centroid, t, mask, false,
                        "CombatSubsystem::_traceHitTarget 2");

        if (trace.startsolid) {
            return false;
        }

        if (!trace.ent) {
            return false;
        }

        if (trace.ent == target->edict) {
            return true;
        }

        if (act->enemyManager->IsValidEnemy(trace.ent->entity)) {
            return true;
        }
    }

    return false;
}



void CombatSubsystem::_init()
{
	_activeWeapon.weapon = NULL;
	_nextTimeTracedToTarget = 0;
	_traceInterval          = DEFAULT_TRACE_INTERVAL;
	_canShootTarget         = false;
	_yawDiff = 0.0f;
}

void CombatSubsystem::UseActorWeapon(const str &weaponName, weaponhand_t hand)
{
	Weapon *weapon;

	if (!stricmp(weaponName.c_str(), "none")) {
		act->DeactivateWeapon(WEAPON_LEFT);
		act->DeactivateWeapon(WEAPON_RIGHT);
		act->DeactivateWeapon(WEAPON_DUAL);
		act->AddStateFlag(STATE_FLAG_CHANGED_WEAPON);
		act->ClearTorsoAnim();
		_activeWeapon.weapon = NULL;

		return;
	}

	weapon = (Weapon *)act->FindItem(weaponName.c_str());

	if (!weapon) {
		act->warning("CombatSubsystem::UseActorWeapon", "Actor does not have weapon %s", weaponName.c_str());
		return;
	}

	if (!_activeWeapon.weapon) {
		_activeWeapon.weapon = weapon;
		_activeWeapon.hand = hand;
		_activeWeapon.weapon->SetOwner(act);
		act->ActivateWeapon(weapon, hand);
		act->AddStateFlag(STATE_FLAG_CHANGED_WEAPON);
		return;
	}

	if (weapon == _activeWeapon.weapon && hand == _activeWeapon.hand) {
		return;
	}

	Weapon *oldweapon = act->GetActiveWeapon(hand);
	if (oldweapon) {
		act->DeactivateWeapon(hand);
	}

	_activeWeapon.weapon = weapon;
	_activeWeapon.hand = hand;
	_activeWeapon.weapon->SetOwner(act);
	act->ActivateWeapon(weapon, hand);
	act->AddStateFlag(STATE_FLAG_CHANGED_WEAPON);
}

WeaponPtr CombatSubsystem::GetBestAvailableWeapon(Entity *target)
{
	Item   *item;
	Weapon *weapon;
	Weapon *bestWeapon;
	float bestPowerRating;
	float powerRating;

	item = NULL;
	weapon = NULL;
	bestWeapon = NULL;
	bestPowerRating = 0.0f;

	item = act->NextItem(item);

	while (item) {
		if (item->isSubclassOf(Weapon)) {
			weapon = (Weapon *)(Item *)item;
			if (weapon) {
				powerRating = getModifiedPowerRating(target, weapon);
				if (powerRating > bestPowerRating) {
					bestPowerRating = powerRating;
					bestWeapon = weapon;
				}
			}
		}

		item = act->NextItem(item);
	}

	return bestWeapon;
}

float CombatSubsystem::getModifiedPowerRating(Entity *target, Weapon *weapon)
{
	float rangeToTarget;
	float weaponPowerRating;
	float weaponRange;
	float weaponSpread;
	float hitPercentage;
	float skillLevel;
	Actor *actTarget;
	Vector selfToTarget;

	if (!target) {
		assert(target != NULL);
		return 0.0f;
	}

	weaponPowerRating = weapon->GetPowerRating();
	weaponRange       = weapon->GetRange(FIRE_MODE1);

	selfToTarget = target->origin - act->origin;
	rangeToTarget = selfToTarget.length();

	if (rangeToTarget > weaponRange) {
		return 0.0f;
	}

	if (target->isSubclassOf(Actor)) {
		actTarget = (Actor *)target;
		if (!actTarget->canBeDamagedBy(weapon->GetMeansOfDeath(FIRE_MODE1))) {
			return 0.0f;
		}
	}

	hitPercentage = 0.0;
	if ((weapon->flags |= FT_BULLET) || (weapon->flags |= FT_MELEE)) {
		weaponSpread = weapon->GetBulletSpreadX(FIRE_MODE1);
		if (weaponSpread < 1.0) {
			weaponSpread = 1.0f;
		}

		hitPercentage = rangeToTarget / weaponRange * weaponSpread;
	} else {
		hitPercentage = weapon->GetProjectileSpeed() / rangeToTarget;
	}

	skillLevel = weapon->GetSkillLevel();

	return hitPercentage * weaponPowerRating * skillLevel;
}

float CombatSubsystem::GetActiveWeaponPowerRating(Entity *target)
{
	if (_activeWeapon.weapon) {
		return getModifiedPowerRating(target, _activeWeapon.weapon);
	}

	return 0.0f;
}

float CombatSubsystem::GetAimGunYaw(const Vector &target)
{
	Vector GunPos, GunForward, GunRight, GunUp;
	Vector GunPosToOrigin, OriginToGunPos, OriginToTarget;
	Vector TagForwardAngles, GunToOriginAngles, OriginToGunAngles, OriginToTargetAngles;

	float AngleDiff;
	float yaw;

	float a, r, d;

	GetGunPositionData(&GunPos, &GunForward, &GunRight, &GunUp);

	GunPosToOrigin = act->origin - GunPos;
	OriginToGunPos = GunPos - act->origin;
	OriginToTarget = target - act->origin;

	GunPosToOrigin.z = 0.0f;
	OriginToTarget.z = 0.0f;
	OriginToGunPos.z = 0.0f;

	a = GunPosToOrigin.length();
	d = OriginToTarget.length();

	TagForwardAngles = GunForward.toAngles();
	GunToOriginAngles = GunPosToOrigin.toAngles();
	OriginToGunAngles = OriginToGunPos.toAngles();
	OriginToTargetAngles = OriginToTarget.toAngles();

	AngleDiff = AngleNormalize180(GunToOriginAngles[YAW] - TagForwardAngles[YAW]);
	AngleDiff = 180.0f - AngleDiff;
	r = a * sin(DEG2RAD(AngleDiff));

	float angleA;
	angleA = RAD2DEG(acos(r / d));

	float angleB;
	angleB = RAD2DEG(acos(r / a));

	float angleC;
	angleC = AngleNormalize180(OriginToTargetAngles[YAW] - OriginToGunAngles[YAW]);

	float totalAngle;
	totalAngle = angleB + angleC;

	float finalAngle;
	finalAngle = totalAngle - angleA;

	yaw = AngleNormalize180(finalAngle);

	return yaw;
}

float CombatSubsystem::GetAimGunPitch(const Vector &target)
{
	Vector GunPos, GunForward, GunRight, GunUp, TagToTarget;
	Vector TagForwardAngles, TagToTargetAngles;

	GetGunPositionData(&GunPos, &GunForward, &GunRight, &GunUp);
	TagToTarget = target - GunPos;

	TagForwardAngles = GunForward.toAngles();
	TagToTargetAngles = TagToTarget.toAngles();

	return AngleNormalize180(TagToTargetAngles[PITCH] - TagForwardAngles[PITCH]);
}

void CombatSubsystem::OverrideSpread(float spreadX, float spreadY)
{
	if (!_activeWeapon.weapon) {
		return;
	}

	_activeWeapon.weapon->SetBulletSpread(spreadX, spreadY);
}




bool CombatSubsystem::CanAttackEnemy()
{
	Entity *currentEnemy = NULL;
	
	currentEnemy = act->enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return false;

	bool attack = CanAttackTarget( currentEnemy );
	return attack;
	
	
}

Vector CombatSubsystem::GetLeadingTargetPos( float projSpeed , Vector originalTargetPos , Entity *target )
{
	Vector newTargetPos = originalTargetPos;

	if ( !target )
		return newTargetPos;

	if ( target->groundentity ) 
		{

		Vector targetVelocity = target->velocity;
		if ( projSpeed <= 0 )
			projSpeed = 1;

		Vector selfToTarget = originalTargetPos - act->centroid;
		float distToTarget = selfToTarget.length();
		float timeToTarget = distToTarget/projSpeed;
		Vector enemyMoveDir = targetVelocity;
		float targetSpeed = targetVelocity.length();

		enemyMoveDir.normalize();

		float leadFraction = act->minLeadFactor + G_Random( act->maxLeadFactor - act->minLeadFactor );
		Vector leadVector = enemyMoveDir * targetSpeed * timeToTarget;
		newTargetPos = originalTargetPos + (leadFraction * leadVector);
		}
	

	return newTargetPos;
}

void CombatSubsystem::SetTraceInterval( const float interval )
{
	_traceInterval = interval;
}

bool CombatSubsystem::UsingWeaponNamed( const str &weaponName )
{
	if ( !_activeWeapon.weapon )
		return false;
	
	
	if ( !Q_stricmp(_activeWeapon.weapon->getName().c_str() , weaponName.c_str() ) )
		return true;
	
	return false;
}

bool CombatSubsystem::WeaponIsFireType( firetype_t fire_type )
{
	firetype_t weapon_fire_type;
	
	if ( !_activeWeapon.weapon )
		return false;
	
	weapon_fire_type = _activeWeapon.weapon->GetFireType( FIRE_MODE1 );
	
	if ( weapon_fire_type == fire_type )
		return true;
	
	
	return false;
}



ActiveWeapon::ActiveWeapon()
   {
   weapon = NULL;
   hand	= WEAPON_ERROR;
   }

void ActiveWeapon::Archive( Archiver &arc )
   {
   arc.ArchiveSafePointer( &weapon );
   ArchiveEnum( hand, weaponhand_t );
   }
