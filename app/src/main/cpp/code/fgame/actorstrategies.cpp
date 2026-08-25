
#include "actor.h"
#include "actorstrategies.h"
#include "gamecvars.h"  
#include "actor_locomotion.h"
#include "actor_posturecontroller.h" 
#include "actor_enemymanager.h"
#include "actorgamecomponents.h"

void ActorThink::DoArchive(Archiver& arc) {}

void ActorThink::CheckGround(Actor& actor)
{
    if (actor.GetActorFlag(ACTOR_FLAG_HAVE_MOVED)
        || (actor.groundentity && actor.groundentity->entity
            && (actor.groundentity->entity->entnum != ENTITYNUM_WORLD))) {
        actor.CheckGround();
    }

    if (actor.groundentity) {
        if (!actor.Immune(MOD_FALLING) && !(actor.flags & FL_FLY)
            && (actor.origin.z + 1000.0f < actor.last_ground_z)) {
            actor.Damage(
                world, world, 1000.0f, actor.origin, vec_zero, vec_zero, 0, DAMAGE_NO_ARMOR, MOD_FALLING
            );
        }

        actor.last_ground_z = actor.origin.z;
    }
}

void ActorThink::TryDrown(Actor& actor)
{
    if (actor.waterlevel == 3 && !(actor.flags & FL_SWIM)) {
        if (actor.air_finished < level.time) {
            actor.movementSubsystem->CheckWater();
            if (actor.waterlevel < 3) {
                actor.air_finished = level.time + 5.0f;
            } else if ((actor.next_drown_time < level.time) && (actor.health > 0)) {
                actor.next_drown_time = level.time + 1.0f;

                actor.BroadcastSound();

                actor.Damage(
                    world, world, 15.0f, actor.origin, vec_zero, vec_zero, 0, DAMAGE_NO_ARMOR, MOD_DROWN
                );
            }
        }
    } else {
        actor.air_finished = level.time + 5.0f;
    }
}

void ActorThink::DoMove(Actor& actor)
{
    if ((actor.flags & FL_IMMOBILE) || (actor.flags & FL_PARTIAL_IMMOBILE)) {
        actor.animate->StopAnimating();
        return;
    }

    actor.movementSubsystem->CalcMove();
    actor.movementSubsystem->setLastMove(STEPMOVE_STUCK);

    stepmoveresult_t lastMove;

    if (actor.flags & FL_SWIM) {
        lastMove = actor.movementSubsystem->WaterMove();
    } else if (actor.flags & FL_FLY) {
        lastMove = actor.movementSubsystem->AirMove();
    } else {
        lastMove = actor.movementSubsystem->TryMove();
    }

    actor.movementSubsystem->setLastMove(lastMove);

    if ((actor.movetype != MOVETYPE_NONE) && (actor.movetype != MOVETYPE_STATIONARY)
        && actor.GetActorFlag(ACTOR_FLAG_TOUCH_TRIGGERS) && actor.GetActorFlag(ACTOR_FLAG_HAVE_MOVED)) {
        G_TouchTriggers(&actor);
    }

    if (actor.groundentity && (actor.groundentity->entity != world) && !M_CheckBottom(&actor)) {
        actor.flags |= FL_PARTIALGROUND;
    }
}

void ActorThink::ActorStateUpdate(Actor& actor)
{
    if (actor.last_origin != actor.origin) {
        actor.SetActorFlag(ACTOR_FLAG_HAVE_MOVED, qtrue);
    } else {
        actor.SetActorFlag(ACTOR_FLAG_HAVE_MOVED, qfalse);
    }

    actor.last_origin = actor.origin;

    if (!(actor.flags & FL_SWIM) && !(actor.flags & FL_FLY) && actor.movementSubsystem
        && actor.movementSubsystem->GetStickToGround()) {
        CheckGround(actor);
    }

    if (!actor.deadflag) {
        actor.CheckStun();

        if (actor.gameComponent) {
            actor.gameComponent->HandleThink();
        }

        if (actor.DialogMode == DIALOG_MODE_ANXIOUS) {
            actor.TryTalkToPlayer();
        }

        if (actor.GetActorFlag(ACTOR_FLAG_SHOULD_BLINK)) {
            actor.TryBlink();
        }

        if ( actor.getHeadWatchAllowed() ) actor.headWatcher->HeadWatchTarget();

        actor.eyeposition[2] = actor.maxs[2] + actor.eyeoffset[2];

        TryDrown(actor);

        if (actor.groundentity && (actor.groundentity->entity != world) && !M_CheckBottom(&actor)) {
            actor.flags |= FL_PARTIALGROUND;
        }
    }
}

void ActorThink::InanimateObject(Actor& actor)
{
    if (actor.behavior && actor.behavior->Evaluate(actor) != BEHAVIOR_EVALUATING) {
        actor.EndBehavior();

        actor.turnThinkOff();
        ActiveList.RemoveObject(&actor);
    }

    if (actor.headBehavior && actor.headBehavior->Evaluate(actor) != BEHAVIOR_EVALUATING) {
        actor.EndHeadBehavior();

        actor.turnThinkOff();
        ActiveList.RemoveObject(&actor);
    }

    if (actor.eyeBehavior && actor.eyeBehavior->Evaluate(actor) != BEHAVIOR_EVALUATING) {
        actor.EndEyeBehavior();

        actor.turnThinkOff();
        ActiveList.RemoveObject(&actor);
    }

    if (actor.torsoBehavior && actor.torsoBehavior->Evaluate(actor) != BEHAVIOR_EVALUATING) {
        actor.EndTorsoBehavior();

        actor.turnThinkOff();
        ActiveList.RemoveObject(&actor);
    }
}

void DefaultThink::Think(Actor& actor)
{
    if ( actor.flags & FL_IMMOBILE )
    {
        if ( ( actor.GetActorFlag( ACTOR_FLAG_UPDATE_BOSS_HEALTH ) && actor.max_boss_health && ( actor.mode == ACTOR_MODE_AI ) ) || actor.GetActorFlag( ACTOR_FLAG_FORCE_LIFEBAR ) )
            UpdateBossHealth( actor );

        if ( actor.statemap )
        {
            actor.last_time_active = level.time;
            return;
        }
    }

    if ( ( actor.GetActorFlag( ACTOR_FLAG_UPDATE_BOSS_HEALTH ) && actor.max_boss_health && ( actor.mode == ACTOR_MODE_AI ) ) || actor.GetActorFlag( ACTOR_FLAG_FORCE_LIFEBAR ) )
        UpdateBossHealth( actor );

    if ( actor.postureController )
        actor.postureController->evaluate();

    ActorStateUpdate( actor );

    if ( !actor.deadflag )
    {
        actor.enemyManager->Update();

        DoMove( actor );

        if ( actor.actortype == IS_INANIMATE )
        {
            InanimateObject( actor );
            return;
        }

        ProcessBehaviors( actor );
    }
}


SimplifiedThink::SimplifiedThink(Actor *actor)
    : _actor(actor)
    , _previousContents(actor ? actor->getContents() : 0)
{
    if (actor) {
        actor->setContents(0);
    }
}

SimplifiedThink::~SimplifiedThink(void)
{
    if (_actor) {
        _actor->setContents(_previousContents);
    }
}

void SimplifiedThink::DoArchive(Archiver& arc)
{
    ActorThink::DoArchive(arc);
    arc.ArchiveInteger(&_previousContents);
    arc.ArchiveSafePointer(&_actor);
}

void SimplifiedThink::DoMove(Actor& actor)
{
    actor.last_origin = actor.origin;
    actor.movementSubsystem->CalcMove();
    actor.movementSubsystem->setLastMove(actor.movementSubsystem->SimpleMove(actor.movementSubsystem->GetStickToGround()));
}

// this strategy and this body never runs in the retail campaign.  It is written
void SimplifiedThink::Think(Actor& actor)
{
    actor.enemyManager->TrivialUpdate();

    if (actor.GetActorFlag(ACTOR_FLAG_SHOULD_BLINK)) {
        actor.TryBlink();
    }

    ProcessBehaviors(actor);

    DoMove(actor);
}


void ActorThink::ProcessBehaviors(Actor& actor)
{
    bool showFailure;
    showFailure = (ai_showfailure->integer != 0);

    actor.ProcessMasterStateMachine();
    actor.ProcessActorStateMachine();

    if (actor.behavior) {
        actor.behaviorCode = actor.behavior->Evaluate(actor);

        if (actor.behaviorCode == BEHAVIOR_FAILED) {
            actor.behaviorFailureReason = actor.behavior->GetFailureReason();
        }

        if (actor.behaviorCode != BEHAVIOR_EVALUATING) {
            if (Q_stricmp(actor.behavior->getClassname(), "Talk") == 0) {
                actor.EndBehavior();
                actor.EndMode();
            } else {
                actor.EndBehavior();
            }
        }

        actor.ProcessActorStateMachine();
    }

    if (actor.headBehavior) {
        actor.headBehaviorCode = actor.headBehavior->Evaluate(actor);
        if (actor.headBehaviorCode != BEHAVIOR_EVALUATING) {
            actor.EndHeadBehavior();
        }
        actor.ProcessActorStateMachine();
    }

    if (actor.eyeBehavior) {
        actor.eyeBehaviorCode = actor.eyeBehavior->Evaluate(actor);
        if (actor.eyeBehaviorCode != BEHAVIOR_EVALUATING) {
            actor.EndEyeBehavior();
        }
        actor.ProcessActorStateMachine();
    }

    if (actor.torsoBehavior) {
        actor.torsoBehaviorCode = actor.torsoBehavior->Evaluate(actor);
        if (actor.torsoBehaviorCode != BEHAVIOR_EVALUATING) {
            actor.EndTorsoBehavior();
        }
        actor.ProcessActorStateMachine();
    }

    actor.SetActorFlag(ACTOR_FLAG_ANIM_DONE, false);
    actor.SetActorFlag(ACTOR_FLAG_TORSO_ANIM_DONE, false);


    if ((actor.newanimnum != -1) || (actor.newTorsoAnimNum != -1)) {
        actor.ChangeAnim();
    }

    if (!showFailure) {
        return;
    }

    if (actor.behaviorCode == BEHAVIOR_FAILED && !actor.GetActorFlag(ACTOR_FLAG_DISPLAYING_FAILURE_FX)) {
        Event *event = new Event(EV_DisplayEffect);
        event->AddString("electric");
        actor.ProcessEvent(event);
        actor.SetActorFlag(ACTOR_FLAG_DISPLAYING_FAILURE_FX, true);
        return;
    } else if (actor.behaviorCode != BEHAVIOR_FAILED && actor.GetActorFlag(ACTOR_FLAG_DISPLAYING_FAILURE_FX)) {
        Event *event = new Event(EV_DisplayEffect);
        event->AddString("noelectric");
        actor.ProcessEvent(event);
        actor.SetActorFlag(ACTOR_FLAG_DISPLAYING_FAILURE_FX, false);
        return;
    }
}

void ActorThink::UpdateBossHealth(Actor& actor)
{
    char bosshealth_string[20];

    sprintf(bosshealth_string, "%.5f", actor.health / actor.max_boss_health);
    gi.cvar_set("bosshealth", bosshealth_string);

    gi.cvar_set("bossname", actor.getName());
}
