
#include "animate.h"
#include "player.h"
#include "qfiles.h"
#include "scriptexception.h"
#include "g_spawn.h"
#include "g_utils.h"
#include "../corepp/tiki.h"

#define R120_PROBE_GET(name, def, flags) gi.cvar(name, def, flags)
#define R120_PROBE_PRINT                 gi.Printf
#include "../qcommon/qa_r120.h"
#define R131_PROBE_GET(name, def, flags) gi.cvar(name, def, flags)
#define R131_PROBE_PRINT                 gi.Printf
#include "../qcommon/qa_r131.h"

static int r131_pin_anim_at[MAX_GENTITIES];
static int r131_pin_clear_since[MAX_GENTITIES];

extern Event EV_Path_SetAnim;

Event EV_SetFrame (    "setframe",
    EV_CODEONLY,
    "iS",
    "frameNumber animName",
    "Set the frame on the legs, if anim is not specified, current is assumed.");

extern Event EV_Behavior_AnimDone;

Event EV_Torso_AnimDone (    "torso_animdone",
    EV_CODEONLY,
    NULL,
    NULL,
    "Torso animation has finished, not for script use.");
Event EV_NewAnim (    "animate_newanim",
    EV_CODEONLY,
    "ii",
    "animNum bodyPart",
    "Start a new animation, not for script use.");
Event EV_FrameDelta (    "setmovedelta",
    EV_CODEONLY,
    "v",
    "moveDelta",
    "movement from animation, not for script use.");
Event EV_StopAnimating (    "stopanimating",
    EV_CODEONLY,
    NULL,
    NULL,
    "stop the legs from animating.  Animation will end at the end of current cycle.");
Event EV_Torso_StopAnimating (    "torso_stopanimating",
    EV_CODEONLY,
    NULL,
    NULL,
    "stop the torso from animating.  Animation will end at the end of current cycle.");



Event EV_Anim (    "anim",
    EV_DEFAULT,
    "s",
    "animName",
    "set the legs animation to animName.");
Event EV_AnimDone (    "animdone",
    EV_CODEONLY,
    NULL,
    NULL,
    "Legs animation has finished, not for script use.");
Event EV_Torso_Anim (    "torso_anim",
    EV_CODEONLY,
    "s",
    "animName",
    "set the torso animation to animName.");
Event EV_Torso_SetFrame (    "torso_setframe",
    EV_CODEONLY,
    "iS",
    "frameNumber animName",
    "Set the frame on the torso, if anim is not specified, current is assumed.");

CLASS_DECLARATION(Listener, Animate, "animate") {
    {&EV_Anim,                     &Animate::Legs_AnimEvent        },
    {&EV_SetFrame,                 &Animate::Legs_SetFrameEvent    },
    {&EV_AnimDone,                 &Animate::Legs_AnimDoneEvent    },
    {&EV_StopAnimating,            &Animate::Legs_StopAnimating    },
    {&EV_FrameDelta,               &Animate::FrameDeltaEvent       },
    {&EV_NewAnim,                  &Animate::NewAnimEvent          },
    {&EV_Torso_Anim,               &Animate::Torso_AnimEvent       },
    {&EV_Torso_SetFrame,           &Animate::Torso_SetFrameEvent   },
    {&EV_Torso_AnimDone,           &Animate::Torso_AnimDoneEvent   },
    {&EV_Torso_StopAnimating,      &Animate::Torso_StopAnimating   },
    {NULL,                         NULL                            }
};


Animate::Animate()
{
    assert(0);

    self = NULL;
}

Animate::Animate(Entity *ent)
{
    self = ent;


    frame_delta = vec_zero;

    legs_animtime  = 0.0f;
    torso_animtime = 0.0f;
    legs_starttime  = 0.0f;
    torso_starttime = 0.0f;
    legs_frametime  = 0.0f;
    torso_frametime = 0.0f;
    legs_numframes  = 0;
    torso_numframes = 0;

    animDoneEvent       = NULL;
    torso_animDoneEvent = NULL;

    if (!self) {
        return;
    }

    self->edict->s.animationRate = 1.0f;

    if (!LoadingSavegame) {
        self->edict->s.anim                  = 0;
        self->edict->s.frame                 = 0;
        self->edict->s.crossblend_time       = 0;
        self->edict->s.torso_anim            = 0;
        self->edict->s.torso_frame           = 0;
        self->edict->s.torso_crossblend_time = 0;

        self->edict->s.animationRate = 1.0f;

        self->flags |= FL_ANIMATE;
    }

    self->edict->s.anim       |= (ANIM_SERVER_EXITCOMMANDS_PROCESSED);
    self->edict->s.torso_anim |= (ANIM_SERVER_EXITCOMMANDS_PROCESSED);
    self->edict->s.effectsAnims[0] = -1;
    self->edict->s.effectsAnims[1] = -1;
    self->edict->s.effectsAnims[2] = -1;
    self->edict->s.effectsAnims[3] = -1;
}

Animate::~Animate()
{
    if (animDoneEvent) {
        delete animDoneEvent;
        animDoneEvent = NULL;
    }
    if (torso_animDoneEvent) {
        delete torso_animDoneEvent;
        torso_animDoneEvent = NULL;
    }
}

void Animate::Archive(Archiver& arc)
{
    int index;

    for (int part = 0; part < 2; part++) {
        Event **ev = (part == 0) ? &animDoneEvent : &torso_animDoneEvent;

        if (arc.Saving()) {
            index = *ev ? ARCHIVE_POINTER_VALID : ARCHIVE_NULL_POINTER;
        }
        arc.ArchiveInteger(&index);
        if (index != ARCHIVE_NULL_POINTER) {
            if (arc.Loading()) {
                *ev = new Event;
            }
            (*ev)->ArchiveRetail(arc);
        }
    }

    arc.ArchiveFloat(&legs_animtime);        
    arc.ArchiveFloat(&torso_animtime);       
    arc.ArchiveFloat(&legs_starttime);       
    arc.ArchiveFloat(&torso_starttime);      
    arc.ArchiveFloat(&legs_frametime);       
    arc.ArchiveFloat(&torso_frametime);      

    arc.ArchiveInteger(&legs_numframes);     
    arc.ArchiveInteger(&torso_numframes);    

    arc.ArchiveString(&currentAnim);          
    arc.ArchiveFloat(&oldAnimationRate);      

    arc.ArchiveVector(&frame_delta);          
}




// SDK animate.cpp:271-275 -- the channel is left completely untouched, not cleared.
void Animate::NewAnim(int animnum, int slot, float weight, int ef2Channel)
{
    tiki_cmd_t cmds;
    int       *anim      = NULL;
    float     *animtime  = NULL;
    float     *starttime = NULL;
    float     *frametime = NULL;
    int       *numframes = NULL;
    int        flags     = 0;
    int        channel;

    (void)weight;

    if (self->edict->s.eType == ET_GENERAL) {
        self->edict->s.eType = ET_MODELANIM;
    }

    if (LoadingSavegame) {
        return;
    }

    // dead m_localFrameInfo[slot] entry for slots >= 2, which retail does not have at all.
    channel = ef2Channel;
    if (channel == EF2ANIM_INFER) {
        channel = (slot == 0) ? EF2ANIM_LEGS : (slot == 1) ? EF2ANIM_TORSO : EF2ANIM_NONE;
    }

    if (channel == EF2ANIM_LEGS) {
        int r131_pin_was = (self->edict->s.frame & FRAME_EXPLICIT) ? 1 : 0;

        if (self->entnum >= 0 && self->entnum < MAX_GENTITIES) {
            r131_pin_clear_since[self->entnum] = 1;
        }

        anim      = &self->edict->s.anim;
        self->edict->s.frame &= ~FRAME_EXPLICIT;
        flags     = EVENT_LEGS_ANIM;
        animtime  = &legs_animtime;
        starttime = &legs_starttime;
        frametime = &legs_frametime;
        numframes = &legs_numframes;

        {
            static int r131_clr;

            if (R131_On() && R131_Throttle(&r131_clr, 4, 8)) {
                R131_A1_PROBE(
                    "anim:clear",
                    "ent=%d etype=%d ch=LEGS newidx=%d pinwas=%d es.frame=0x%x "
                    "es.anim=%d svtime=%d model='%s'\n",
                    self->entnum,
                    self->edict->s.eType,
                    animnum,
                    r131_pin_was,
                    self->edict->s.frame,
                    self->edict->s.anim,
                    level.inttime,
                    self->model.c_str());
            }
        }
    } else if (channel == EF2ANIM_TORSO) {
        anim      = &self->edict->s.torso_anim;
        self->edict->s.torso_frame &= ~FRAME_EXPLICIT;
        flags     = EVENT_TORSO_ANIM;
        animtime  = &torso_animtime;
        starttime = &torso_starttime;
        frametime = &torso_frametime;
        numframes = &torso_numframes;
    } else {
        warning("NewAnim", "Unknown body part %d", slot);
        return;
    }

    // commands and re-armed a crossblend that retail does not start.  Mac: 0xAF6FC
    int   last_anim       = *anim & ANIM_MASK;
    int   last_anim_flags = *anim;
    float lastStartTime   = *starttime;

    // ANIM_SERVER_EXITCOMMANDS_PROCESSED latch is still clear (Mac 0xAF70C `cmpw r0,r18'
    if ((last_anim != animnum) && !(last_anim_flags & ANIM_SERVER_EXITCOMMANDS_PROCESSED)) {
        if (gi.Frame_Commands(self->edict->s.modelindex, last_anim, TIKI_FRAME_EXIT, &cmds)) {
            for (int ii = 0; ii < cmds.num_cmds; ii++) {
                Event *ev = new Event(cmds.cmds[ii].args[0]);

                ev->SetSource(EV_FROM_ANIMATION);
                ev->SetAnimationNumber(last_anim);
                ev->SetAnimationFrame(0);

                for (int j = 1; j < cmds.cmds[ii].num_args; j++) {
                    ev->AddToken(cmds.cmds[ii].args[j]);
                }
                self->ProcessEvent(ev);
            }
        }
    }

    // exit block above.  Retail's store at 0xAF70C..0xAF878 never sets that bit (it is the
    // therefore used to leave the latch set here -- one instruction retail does not have,
    if (animnum >= 0 && animnum < gi.NumAnims(self->edict->s.modelindex)) {
        if (*starttime == level.time) {
            *anim = (*anim & ANIM_TOGGLEBIT) | animnum | ANIM_BLEND;
        } else {
            *anim = ((*anim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT) | animnum | ANIM_BLEND;
        }
    } else {
        return;
    }

    CancelFlaggedEvents(flags);
    self->CancelFlaggedEvents(flags);

    float totaltime = gi.Anim_Time(self->edict->s.modelindex, animnum) / self->edict->s.animationRate;
    *animtime       = totaltime;
    *starttime      = level.time;
    *numframes      = gi.Anim_NumFrames(self->edict->s.modelindex, animnum);
    *frametime      = gi.Frame_Time(self->edict->s.modelindex, animnum, 0) / self->edict->s.animationRate;

    qboolean hascommands = (gi.Anim_HasCommands(self->edict->s.modelindex, animnum) != 0);

    {
        static int r121_has;
        if (R120_Throttle(&r121_has, 8, 500)) {
            R121_PROBE("sanim:hascmds", "ent=%d modelindex=%d anim=%d hascommands=%d\n",
                       self->entnum, self->edict->s.modelindex, animnum, hascommands ? 1 : 0);
        }
    }

    // entry, not per-frame.  Retail sets it on dead actors (SDK actor.cpp:7035) so a corpse's
    if (self->edict->s.eFlags & EF_DONT_PROCESS_COMMANDS) {
        static int r121_dp;
        if (R120_Throttle(&r121_dp, 4, 500)) {
            R121_PROBE("sanim:dontprocess", "ent=%d anim=%d commands CLEARED by EF_DONT_PROCESS_COMMANDS\n",
                       self->entnum, animnum);
        }
        hascommands = qfalse;
    }

    if (((last_anim != animnum) || (lastStartTime == 0.0f)) && hascommands) {
        ClearAllEffectAnims();

        if (gi.Frame_Commands(self->edict->s.modelindex, animnum, TIKI_FRAME_ENTRY, &cmds)) {
            int ii, j;

            {
                static int r121_entry;
                if (R120_Throttle(&r121_entry, 8, 500)) {
                    R121_PROBE("sanim:entry", "ent=%d anim=%d ENTRY cmds=%d\n",
                               self->entnum, animnum, cmds.num_cmds);
                }
            }

            for (ii = 0; ii < cmds.num_cmds; ii++) {
                const tiki_singlecmd_t& single_cmd = cmds.cmds[ii];

                {
                    static int r121_scmd;
                    if (R120_Throttle(&r121_scmd, 24, 400)) {
                        R121_PROBE("sanim:cmd", "ent=%d anim=%d cmd='%s' nargs=%d\n",
                                   self->entnum, animnum, single_cmd.args[0], single_cmd.num_args);
                    }
                }


                Event *ev = new Event(single_cmd.args[0]);

                ev->SetSource(EV_FROM_ANIMATION);
                ev->SetAnimationNumber(animnum);
                ev->SetAnimationFrame(0);

                for (j = 1; j < single_cmd.num_args; j++) {
                    ev->AddToken(single_cmd.args[j]);
                }

                self->ProcessEvent(ev);
            }
        }
    }

    // deliberately NOT set here: retail's pack rebuilds the word from

    qboolean dodelta = qfalse;
    if (slot == 0) {
        Vector totaldelta;
        gi.Anim_AbsoluteDelta(self->edict->s.modelindex, animnum, totaldelta);

        float length = totaldelta.length();
        if (length > MINIMUM_DELTA_MOVEMENT) {
            int deltaflags = gi.Anim_Flags(self->edict->s.modelindex, animnum);
            if (!(deltaflags & MDL_ANIM_DELTA_DRIVEN)) {
                length /= (float)(*numframes);
                if (length > MINIMUM_DELTA_MOVEMENT_PER_FRAME) {
                    dodelta = qtrue;
                }
            }
        }
    }

    // Entry and exit commands are deliberately NOT part of this loop: retail gates entry
    if (hascommands || dodelta) {
        float time      = 0.0f;
        int   postflags = flags;

        for (int i = 0; i < *numframes; i++) {
            if (hascommands) {
                if (gi.Frame_Commands(self->edict->s.modelindex, animnum, i, &cmds)) {
                    for (int ii = 0; ii < cmds.num_cmds; ii++) {
                        Event *ev = new Event(cmds.cmds[ii].args[0]);

                        ev->SetSource(EV_FROM_ANIMATION);
                        ev->SetAnimationNumber(animnum);
                        ev->SetAnimationFrame(i);

                        for (int j = 1; j < cmds.cmds[ii].num_args; j++) {
                            ev->AddToken(cmds.cmds[ii].args[j]);
                        }
                        self->PostEvent(ev, time, postflags);
                    }
                }
            }

            time += *frametime;

            if (dodelta) {
                Vector delta;

                gi.Frame_Delta(self->edict->s.modelindex, animnum, i, delta);

                if (*frametime > FRAMETIME) {
                    float time_offset;

                    VectorScale(delta, (FRAMETIME / *frametime), delta);

                    for (time_offset = 0; time_offset < *frametime; time_offset += FRAMETIME) {
                        Event *ev = new Event(EV_FrameDelta);
                        ev->AddVector(delta);
                        PostEvent(ev, time + time_offset, postflags);
                    }
                } else {
                    Event *ev = new Event(EV_FrameDelta);
                    ev->AddVector(delta);
                    ev->AddFloat(*frametime);
                    PostEvent(ev, time, postflags);
                }
            }
        }
    }

    {
        if (*numframes == 1 && !hascommands && !self->isSubclassOf(Player)
            && (*frametime <= FRAMETIME)) {
            switch (slot) {
            case 0:
                if (animDoneEvent) {
                    self->PostEvent(*animDoneEvent, 0.0f);
                }
                break;
            case 1:
                if (torso_animDoneEvent) {
                    self->PostEvent(*torso_animDoneEvent, 0.0f);
                }
                break;
            default:
                break;
            }
            return;
        }

        if (slot != 1) {
            if (totaltime > *frametime && last_anim == animnum) {
                PostEvent(EV_AnimDone, totaltime - *frametime, flags);
            } else {
                PostEvent(EV_AnimDone, totaltime, flags);
            }
        } else {
            if (totaltime > *frametime && last_anim == animnum) {
                PostEvent(EV_Torso_AnimDone, totaltime - *frametime, flags);
            } else {
                PostEvent(EV_Torso_AnimDone, totaltime, flags);
            }
        }
    }
}


void Animate::NewAnim(int animnum, Event& newevent, int slot, float weight)
{
    SetAnimDoneEvent(newevent, (slot == 1) ? torso : legs);
    NewAnim(animnum, slot);
}

void Animate::NewAnim(int animnum, Event *newevent, int slot, float weight)
{
    SetAnimDoneEvent(newevent, (slot == 1) ? torso : legs);
    NewAnim(animnum, slot);
}


void Animate::SetAnimDoneEvent( Event *event, bodypart_t part )
{
	Event **doneevent;

	switch( part )
	{
	case legs:
		doneevent = &animDoneEvent;
		break;
	case torso:
		doneevent = &torso_animDoneEvent;
		break;
	default:
		warning( "SetAnimDoneEvent", "Unknown body part %d", part );
		return;
		break;
	}
	if ( *doneevent )
	{
		delete *doneevent;
	}

	*doneevent = event;
}

void Animate::SetAnimDoneEvent( const Event &event, bodypart_t part )
{
	SetAnimDoneEvent( new Event( event ), part );
}

void Animate::EndAnim(int part)
{
    Event **doneEv;
    Event  *ev;

    switch (part) {
    case 0: 
        doneEv = &animDoneEvent;
        break;
    case 1: 
        doneEv = &torso_animDoneEvent;
        break;
    default:
        return;
    }

    if (*doneEv) {
        self->PostEvent(**doneEv, 0.0f);
    }

    ev = new Event(EV_NewAnim);
    if (part == 0) {
        ev->AddInteger(self->edict->s.anim);
    } else {
        ev->AddInteger(self->edict->s.torso_anim);
    }
    ev->AddInteger(part);

    if (part == 0) {
        if (legs_animtime > legs_frametime) {
            PostEvent(ev, legs_frametime, EVENT_LEGS_ANIM);
        } else {
            PostEvent(ev, 0.0f, EVENT_LEGS_ANIM);
        }
    } else {
        if (torso_animtime > torso_frametime) {
            PostEvent(ev, torso_frametime, EVENT_TORSO_ANIM);
        } else {
            PostEvent(ev, 0.0f, EVENT_TORSO_ANIM);
        }
    }
}

void Animate::Legs_AnimDoneEvent(Event *ev)
{
    EndAnim(0);
}

void Animate::Torso_AnimDoneEvent(Event *ev)
{
    EndAnim(1);
}

void Animate::NewAnimEvent(Event *ev)
{
    int animnum = ev->GetInteger(1) & ANIM_MASK;
    int part    = ev->GetInteger(2);
    NewAnim(animnum, part, 1.0f, EF2ANIM_INFER);
}

void Animate::Legs_SetFrameEvent(Event *ev)
{
    int framenum;
    int animnum;

    framenum = ev->GetInteger(1);
    if (ev->NumArgs() > 1) {
        animnum = gi.Anim_NumForName(self->edict->s.modelindex, ev->GetString(2));
    } else {
        animnum = -1;
    }

    SetFrame(framenum, legs, animnum);
}

void Animate::SetFrame(int framenum, bodypart_t part, int anim)
{
    int   flags;
    int   numframes;
    int  *panim;
    int  *frame;

    if (part == torso) {
        frame     = &self->edict->s.torso_frame;
        flags     = EVENT_TORSO_ANIM;
        numframes = torso_numframes;
        panim     = &self->edict->s.torso_anim;
    } else if (part == legs) {
        frame     = &self->edict->s.frame;
        flags     = EVENT_LEGS_ANIM;
        numframes = legs_numframes;
        panim     = &self->edict->s.anim;
    } else {
        warning("SetFrame", "Unknown body part %d", part);
        return;
    }

    if (anim >= 0) {
        numframes = gi.Anim_NumFrames(self->edict->s.modelindex, anim);
    }

    if (framenum < 0 || (framenum >= numframes)) {
        warning("SetFrame", "Frame %d is out of range on model %s", framenum, self->model.c_str());
        return;
    }

    CancelFlaggedEvents(flags);
    self->CancelFlaggedEvents(flags);

    int r131_prevpin = (*frame & FRAME_EXPLICIT) ? 1 : 0;

    *frame = framenum | FRAME_EXPLICIT;

    if (anim >= 0) {
        *panim = anim | ANIM_BLEND;
    }

    {
        static int r131_pin[MAX_GENTITIES];
        int        r131_pin_anim_at_v     = -1;
        int        r131_pin_clear_since_v = 0;

        if (self->entnum >= 0 && self->entnum < MAX_GENTITIES) {
            r131_pin_anim_at_v     = r131_pin_anim_at[self->entnum];
            r131_pin_clear_since_v = r131_pin_clear_since[self->entnum];
        }

        if (R131_On() && self->entnum >= 0 && self->entnum < MAX_GENTITIES
            && R131_Throttle(&r131_pin[self->entnum], 4, 8)) {
            R131_A1_PROBE(
                "anim:pin",
                "ent=%d etype=%d part=%d animidx=%d pin=1 frame=%d prevpin=%d prevanim=%d "
                "clearsince=%d es.anim=%d es.frame=0x%x svtime=%d model='%s'\n",
                self->entnum,
                self->edict->s.eType,
                part,
                anim,
                framenum,
                r131_prevpin,
                r131_pin_anim_at_v,
                r131_pin_clear_since_v,
                self->edict->s.anim,
                self->edict->s.frame,
                level.inttime,
                self->model.c_str());
        }

        if (self->entnum >= 0 && self->entnum < MAX_GENTITIES) {
            r131_pin_anim_at[self->entnum]     = anim;
            r131_pin_clear_since[self->entnum] = 0;
        }
    }
}

void Animate::RandomAnimate(const char *animname, Event *endevent, bodypart_t part)
{
    Event   *event_to_post;
    int      num;
    int      flags;
    qboolean allparts;

    assert(animname);
    if (!animname) {
        return;
    }

    allparts = qfalse;
    if (part == all) {
        allparts = qtrue;
    }

    do {
        if (allparts) {
            switch (part) {
            case all:
                part = legs;
                break;
            case legs:
                part = torso;
                break;
            case torso:
                return;
            }
        }

        switch (part) {
        case legs:
            flags = EVENT_LEGS_ANIM;
            break;
        case torso:
            flags = EVENT_TORSO_ANIM;
            break;
        default:
            warning("RandomAnimate", "Unknown body part %d", part);
            return;
        }

        num = gi.Anim_Random(self->edict->s.modelindex, animname);
        currentAnim = animname;

        if (allparts && (part != legs)) {
            event_to_post = endevent ? new Event(endevent) : NULL;
        } else {
            event_to_post = endevent;
        }

        if (num == -1) {
            if (event_to_post) {
                self->PostEvent(event_to_post, FRAMETIME, flags);
            }
        } else {
            SetAnimDoneEvent(event_to_post, part);
            NewAnim(num, (int)part);
        }
    } while (allparts);
}

qboolean Animate::HasAnim(const char *animname)
{
    int num;

    num = gi.Anim_Random(self->edict->s.modelindex, animname);
    return (num >= 0);
}

void Animate::StopAnimating(int slot)
{
    int frame;
    int anim;

    if (slot == all) {
        frame = CurrentFrame(legs);
        anim  = CurrentAnim(legs);
        SetFrame(frame, legs, anim);

        frame = CurrentFrame(torso);
        anim  = CurrentAnim(torso);
        SetFrame(frame, torso, anim);
    } else {
        frame = CurrentFrame(slot);
        anim  = CurrentAnim(slot);
        SetFrame(frame, (bodypart_t)slot, anim);
    }
}

void Animate::ClearTorsoAnim(void)
{
    tiki_cmd_t      cmds;
    int             last_anim;
    static qboolean clearing = false;
    int             slot;

    slot = EF2ANIM_TORSO;
    if (slot < 0) {
        return;
    }

    last_anim = self->edict->s.torso_anim & ANIM_MASK;

    if ((self->edict->s.torso_anim & ANIM_BLEND)
        && !(self->edict->s.torso_anim & ANIM_SERVER_EXITCOMMANDS_PROCESSED)
        && !clearing) {
        if (gi.Frame_Commands(self->edict->s.modelindex, last_anim, TIKI_FRAME_EXIT, &cmds)) {
            int ii, j;

            clearing = true;
            for (ii = 0; ii < cmds.num_cmds; ii++) {
                Event *ev = new Event(cmds.cmds[ii].args[0]);

                ev->SetSource(EV_FROM_ANIMATION);
                ev->SetAnimationNumber(last_anim);
                ev->SetAnimationFrame(0);

                for (j = 1; j < cmds.cmds[ii].num_args; j++) {
                    ev->AddToken(cmds.cmds[ii].args[j]);
                }
                self->ProcessEvent(ev);
            }
            clearing = false;
        }
        self->edict->s.torso_anim |= ANIM_SERVER_EXITCOMMANDS_PROCESSED;
    }

    CancelFlaggedEvents(1 << slot);
    self->CancelFlaggedEvents(1 << slot);
    self->edict->s.torso_anim &= ~ANIM_BLEND;
}

void Animate::ClearLegsAnim(void)
{
    tiki_cmd_t      cmds;
    int             last_anim;
    int             slot;

    slot = EF2ANIM_LEGS;
    if (slot < 0) {
        return;
    }

    if (self->edict->s.anim & ANIM_SERVER_EXITCOMMANDS_PROCESSED) {
        last_anim = self->edict->s.anim & ANIM_MASK;

        if (gi.Frame_Commands(self->edict->s.modelindex, last_anim, TIKI_FRAME_EXIT, &cmds)) {
            int ii, j;

            for (ii = 0; ii < cmds.num_cmds; ii++) {
                Event *ev = new Event(cmds.cmds[ii].args[0]);

                ev->SetSource(EV_FROM_ANIMATION);
                ev->SetAnimationNumber(last_anim);
                ev->SetAnimationFrame(0);

                for (j = 1; j < cmds.cmds[ii].num_args; j++) {
                    ev->AddToken(cmds.cmds[ii].args[j]);
                }
                self->ProcessEvent(ev);
            }
        }
        self->edict->s.anim |= ANIM_SERVER_EXITCOMMANDS_PROCESSED;
    }

    CancelFlaggedEvents(1 << slot);
    self->CancelFlaggedEvents(1 << slot);

    self->edict->s.anim &= ~ANIM_BLEND;
}


int Animate::CurrentAnim(int slot)
{
    switch (slot) {
    case legs:
        if (self->edict->s.anim & ANIM_BLEND) {
            return self->edict->s.anim & ANIM_MASK;
        } else {
            return -1;
        }
        break;

    case torso:
        if (self->edict->s.torso_anim & ANIM_BLEND) {
            return self->edict->s.torso_anim & ANIM_MASK;
        } else {
            return -1;
        }
        break;

    default:
        warning("CurrentAnim", "Unknown body part %d", slot);
        return -1;
        break;
    }
}



const char *Animate::AnimName(int slot)
{
    return gi.Anim_NameForNum(self->edict->s.modelindex, CurrentAnim(slot));
}




















int Animate::CurrentFrame(int part) const
{
    const int *pframe;
    int        numframes;
    float      animtime;
    int        frame;

    if (part == EF2ANIM_TORSO) {
        pframe = &self->edict->s.torso_frame;
    } else if (part == EF2ANIM_LEGS) {
        pframe = &self->edict->s.frame;
    } else {
        gi.WDPrintf("Animate::CurrentFrame : Unknown body part %d\n", part);
        return 0;
    }

    if (*pframe & FRAME_EXPLICIT) {
        return *pframe & FRAME_MASK;
    }

    // `*panim` is still the channel selector above; retail never re-derives an anim

    if (part == EF2ANIM_TORSO) {
        numframes = torso_numframes;
        animtime  = torso_animtime;
    } else {
        numframes = legs_numframes;
        animtime  = legs_animtime;
    }

    if (numframes) {
        frame = (int)((float)((level.time - ((part == EF2ANIM_TORSO) ? torso_starttime : legs_starttime))
                              * numframes)
                      / animtime + 0.5f);

        while (frame >= numframes) {
            frame -= numframes;
        }
    } else {
        frame = 0;
    }

    return frame;
}



void Animate::AddEffectAnim( const char *animname )
{
	int num = gi.Anim_Random( self->edict->s.modelindex, animname );
	if ( num != -1 )
	{
		for ( int effectAnimIdx = 0; effectAnimIdx < NUM_EFFECTS_ANIMS; ++effectAnimIdx )
		{
			int effectAnimNum = self->edict->s.effectsAnims[ effectAnimIdx ] ;
			if ( effectAnimNum == num ) return ;
			if ( effectAnimNum == -1 )
			{
				self->edict->s.effectsAnims[ effectAnimIdx ] = num ;
				return ;
			}
		}
	}
}


void Animate::ClearAllEffectAnims( void )
{
	for ( int effectAnimIdx = 0; effectAnimIdx < NUM_EFFECTS_ANIMS; ++effectAnimIdx )
	{
		self->edict->s.effectsAnims[ effectAnimIdx ] = -1 ;
	}
}


void Animate::FrameDeltaEvent( Event *ev )
{
	frame_delta = ev->GetVector( 1 );

	if (getenv("EF2_DEADWEDGE") && gi.cvar("qa_r120", "0", 0)->integer) {
		static int r133_nd_count[MAX_GENTITIES];
		int       r133_nd_e = self->edict - g_entities;

		if (r133_nd_e >= 0 && r133_nd_e < MAX_GENTITIES && r133_nd_count[r133_nd_e] < 8) {
			int r133_nd_bad = 0;

			for (int r133_nd_i = 0; r133_nd_i < 3; r133_nd_i++) {
				if (!(frame_delta[r133_nd_i] == frame_delta[r133_nd_i])) {
					r133_nd_bad |= 1;
				}
				if (!(self->edict->s.scale == self->edict->s.scale)) {
					r133_nd_bad |= 2;
				}
			}

			if (r133_nd_bad) {
				r133_nd_count[r133_nd_e]++;
				gi.Printf("[R133:nandelta] ent=%d bad=%d anim=%d frame=%d "
				          "delta=(%.6g,%.6g,%.6g) scale=%.6g td=(%.6g,%.6g,%.6g) "
				          "org=(%.1f,%.1f,%.1f) mv=%d\n",
				          r133_nd_e,
				          r133_nd_bad,
				          self->CurrentAnim(legs),
				          self->CurrentFrame(legs),
				          frame_delta[0], frame_delta[1], frame_delta[2],
				          self->edict->s.scale,
				          self->total_delta[0], self->total_delta[1], self->total_delta[2],
				          self->origin[0], self->origin[1], self->origin[2],
				          (int)self->movetype);
			}
		}
	}

	self->total_delta += frame_delta * self->edict->s.scale ;
}


void Animate::Legs_AnimEvent( Event *ev )
{
	RandomAnimate( ev->GetString( 1 ), NULL, legs );
}


void Animate::Legs_StopAnimating( Event *ev )
{
	CancelFlaggedEvents( EVENT_LEGS_ANIM );
	self->CancelFlaggedEvents( EVENT_LEGS_ANIM );
}


void Animate::RemoveEffectAnim( const char *animname )
{
	int num = gi.Anim_Random( self->edict->s.modelindex, animname );
	if ( num != -1 )
	{
		for ( int effectAnimIdx = 0; effectAnimIdx < NUM_EFFECTS_ANIMS; ++effectAnimIdx )
		{
			if ( self->edict->s.effectsAnims[ effectAnimIdx ] == num )
			{
				self->edict->s.effectsAnims[ effectAnimIdx ] = -1 ;
			}
		}
	}
}


void Animate::RestoreAnimationRate( void ) 
{ 
	self->edict->s.animationRate = oldAnimationRate; 
}


void Animate::SetAnimationRate( const float animationRate ) 
{ 
	oldAnimationRate = self->edict->s.animationRate; 
	self->edict->s.animationRate = animationRate;
}


void Animate::StopAnimatingAtEnd( bodypart_t part )
{
    {
        static int r131_stop[ MAX_GENTITIES ];

        if ( R131_On() && self && self->entnum >= 0 && self->entnum < MAX_GENTITIES
             && R131_Throttle( &r131_stop[ self->entnum ], 4, 8 ) )
        {
            R131_A1_PROBE(
                "anim:stopcaller",
                "ent=%d etype=%d part=%d curanim=%d curframe=%d numframes=%d svtime=%d "
                "model='%s'\n",
                self->entnum,
                self->edict->s.eType,
                part,
                CurrentAnim( part ),
                CurrentFrame( part ),
                ( part == torso ) ? torso_numframes : legs_numframes,
                level.inttime,
                self->model.c_str() );
        }
    }

	int anim;
	
	if ( part == all )
	{
		StopAnimatingAtEnd( legs );
		StopAnimatingAtEnd( torso );
	}
	else if ( part == legs )
	{
		anim = CurrentAnim( part );
		SetFrame( legs_numframes - 1, part, anim );
	}
	else if ( part == torso )
	{
		anim = CurrentAnim( part );
		SetFrame( torso_numframes - 1, part, anim );
	}
}


void Animate::Torso_AnimEvent( Event *ev )
{
	RandomAnimate( ev->GetString( 1 ), NULL, torso );
}


void Animate::Torso_SetFrameEvent( Event *ev )
{
	int framenum;
	int animnum;
	
	framenum = ev->GetInteger( 1 );
	if ( ev->NumArgs() > 1 )
	{
		animnum = gi.Anim_NumForName( self->edict->s.modelindex, ev->GetString( 2 ) );
	}
	else
	{
		animnum = -1;
	}
	
	SetFrame( framenum, torso, animnum );
}


void Animate::Torso_StopAnimating( Event *ev )
{
	CancelFlaggedEvents( EVENT_TORSO_ANIM );
	self->CancelFlaggedEvents( EVENT_TORSO_ANIM );
}




str Animate::GetName()
{
	return currentAnim;
}

