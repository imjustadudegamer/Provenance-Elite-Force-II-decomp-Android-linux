/*
===========================================================================
Copyright (C) 2015 the OpenMoHAA team

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


#include "beam.h"
#include "../qcommon/qfiles.h"
#include "game.h"
#include <utility>

Event EV_FuncBeam_Activate (	"activate",
	EV_SCRIPTONLY,
	NULL,
	NULL,
	"Activate the beam"
);



Event EV_FuncBeam_Deactivate (   "deactivate",
   EV_SCRIPTONLY,
   NULL,
   NULL,
   "Deactivate the beam");
Event EV_FuncBeam_Diameter (   "diameter",
   EV_SCRIPTONLY,
   "f",
   "diameter",
   "Set the diameter of the beam");
Event EV_FuncBeam_Maxoffset (   "maxoffset",
   EV_SCRIPTONLY,
   "f",
   "max_offset",
   "Set the maximum offset the beam can travel above, below, forward or back of it's endpoints");
Event EV_FuncBeam_Overlap (   "overlap",
   EV_SCRIPTONLY,
   "f",
   "beam_overlap",
   "Set the amount of overlap the beams have when they are being strung together");
Event EV_FuncBeam_Color (   "color",
   EV_SCRIPTONLY,
   "v[0,1][0,1][0,1]",
   "color_beam",
   "Set the color of the beam");
Event EV_FuncBeam_SetTarget (   "target",
   EV_SCRIPTONLY,
   "s",
   "beam_target",
   "Set the target of the beam. The beam will be drawn from the origin\n"
    "to the origin of the target entity");
Event EV_FuncBeam_SetEndPoint (   "endpoint",
   EV_SCRIPTONLY,
   "v",
   "beam_end_point",
   "Set the end point of the beam. The beam will be draw from the origin to\n"
    "the end point.");
Event EV_FuncBeam_SetLife (   "life",
   EV_DEFAULT,
   "f",
   "beam_life",
   "Set the amount of time the beam stays on when activated");
Event EV_FuncBeam_Shader (   "shader",
   EV_SCRIPTONLY,
   "s",
   "beam_shader",
   "Set the shader that the beam will use");
Event EV_FuncBeam_TileShader (   "tileshader",
   EV_SCRIPTONLY,
   "s",
   "beam_shader",
   "Set the shader that the beam will use.  This shader will be tiled.");
Event EV_FuncBeam_Segments (   "numsegments",
   EV_SCRIPTONLY,
   "i",
   "numsegments",
   "Set the number of segments for the beam");
extern Event EV_Trigger_SetDelay;
Event EV_FuncBeam_NumSphereBeams (   "numspherebeams",
   EV_SCRIPTONLY,
   "i",
   "num",
   "Set the number of beams that will be shot out in a sphere like formation");
Event EV_FuncBeam_SphereRadius (   "radius",
   EV_SCRIPTONLY,
   "f",
   "radius",
   "Set the starting radius of the beams if this is a beamsphere");
Event EV_FuncBeam_ToggleDelay (   "toggledelay",
   EV_SCRIPTONLY,
   "fB",
   "time isRandom",
   "Causes a beam toggling effect.  Sets the time between toggling.\nIf the isRandom flag is set, the time will be random from 0 to the time specified.");
Event EV_FuncBeam_FindEndpoint (   "findendpoint",
   EV_CODEONLY,
   NULL,
   NULL,
   "Find the endpoint of a beam");
Event EV_FuncBeam_UpdateEndpoint (   "updateendpoint",
   EV_CODEONLY,
   NULL,
   NULL,
   "Update the endpoint of a beam");
Event EV_FuncBeam_UpdateOrigin (   "updateorigin",
   EV_CODEONLY,
   NULL,
   NULL,
   "Update the origin of a beam");
Event EV_FuncBeam_EndAlpha (   "endalpha",
   EV_SCRIPTONLY,
   "f",
   "alpha",
   "Set the endpoint alpha value of the beam");
Event EV_FuncBeam_Shoot (   "shoot",
   EV_DEFAULT,
   NULL,
   NULL,
   "Make the beam cause damage to entities that get in the way");
Event EV_FuncBeam_ShootRadius (   "shootradius",
   EV_SCRIPTONLY,
   "f",
   "radius",
   "Set the radius of the damage area between beam endpoints");
Event EV_FuncBeam_Persist (   "persist",
   EV_SCRIPTONLY,
   "b",
   "bool",
   "Set the persist property of the beam");

Event EV_FuncBeam_Minoffset
(
	"minoffset",
	EV_SCRIPTONLY,
	"f",
	"min_offset",
	"Set the minimun offset the beam can travel above, below, forward or back of it's endpoints"
);

Event EV_FuncBeam_Delay
(
	"delay",
	EV_SCRIPTONLY,
	"f",
	"delay",
	"Set the amount of delay on the beam updater"
);

CLASS_DECLARATION(ScriptSlave, FuncBeam, "func_beam") {
    {&EV_FuncBeam_SetTarget,      &FuncBeam::SetTarget        },
    {&EV_Activate,                &FuncBeam::Activate         },
    {&EV_FuncBeam_Activate,       &FuncBeam::Activate         },
    {&EV_SetAngle,                &FuncBeam::SetAngle         },
    {&EV_SetAngles,               &FuncBeam::SetAngles        },
    {&EV_FuncBeam_Deactivate,     &FuncBeam::Deactivate       },
    {&EV_FuncBeam_Maxoffset,      &FuncBeam::SetMaxoffset     },
    {&EV_FuncBeam_Minoffset,      &FuncBeam::SetMinoffset     },
    {&EV_FuncBeam_Overlap,        &FuncBeam::SetOverlap       },
    {&EV_FuncBeam_Color,          &FuncBeam::SetColor         },
    {&EV_FuncBeam_Segments,       &FuncBeam::SetSegments      },
    {&EV_FuncBeam_SetEndPoint,    &FuncBeam::SetEndPoint      },
    {&EV_Model,                   &FuncBeam::SetModel         },
    {&EV_Damage,                  &FuncBeam::SetDamage        },
    {&EV_FuncBeam_SetLife,        &FuncBeam::SetLife          },
    {&EV_FuncBeam_Shader,         &FuncBeam::SetBeamShader    },
    {&EV_FuncBeam_TileShader,     &FuncBeam::SetBeamTileShader},
    {&EV_FuncBeam_Delay,          &FuncBeam::SetDelay         },
    {&EV_FuncBeam_NumSphereBeams, &FuncBeam::SetNumSphereBeams},
    {&EV_FuncBeam_SphereRadius,   &FuncBeam::SetSphereRadius  },
    {&EV_FuncBeam_ToggleDelay,    &FuncBeam::SetToggleDelay   },
    {&EV_FuncBeam_ShootRadius,    &FuncBeam::SetShootRadius   },
    {&EV_FuncBeam_EndAlpha,       &FuncBeam::SetEndAlpha      },
    {&EV_FuncBeam_Persist,        &FuncBeam::SetPersist       },
    {&EV_FuncBeam_FindEndpoint,   &FuncBeam::FindEndpoint     },
    {&EV_FuncBeam_UpdateEndpoint, &FuncBeam::UpdateEndpoint   },
    {&EV_FuncBeam_UpdateOrigin,   &FuncBeam::UpdateOrigin     },
    {&EV_FuncBeam_Shoot,          &FuncBeam::Shoot            },
    {NULL,                        NULL                        }
};

FuncBeam::FuncBeam()
{
    setSolidType(SOLID_NOT);
    setOrigin();

    damage         = 0;
    life           = 0;
    shootradius    = 0;
    use_angles     = false;
    m_bIgnoreWalls = false;

    edict->s.renderfx |= RF_BEAM;
    edict->s.eType      = ET_BEAM; 
    edict->s.modelindex = 1;       
    SetBeamShader("beamshader");

    if (!LoadingSavegame) {
        edict->s.alpha             = 1;  
        edict->s.surfaces[4]       = 4;  
        edict->s.surfaces[0]       = 16; 
        edict->s.surfaces[9]       = 16; 
        edict->s.parent       = ENTITYNUM_NONE;
        edict->s.bone_angles[0][1] = 5; 

        if (spawnflags & 0x0001) { 
            PostEvent(EV_Activate, EV_POSTSPAWN);
        } else {
            hideModel();
        }
        edict->s.skinNum = 0;
        if (spawnflags & 0x0002) {
            edict->s.skinNum |= BEAM_PERSIST_EFFECT;
        }
        if (spawnflags & 0x0004) {
            edict->s.skinNum |= BEAM_WAVE_EFFECT;
        }
        if (spawnflags & 0x0008) {
            edict->s.skinNum |= BEAM_USE_NOISE;
        }

        PostEvent(EV_FuncBeam_FindEndpoint, EV_LINKBEAMS);
    }
}

void FuncBeam::SetEndAlpha(Event *ev)
{
    BEAM_PARM_TO_PKT(ev->GetFloat(1), edict->s.surfaces[9]);
}

void FuncBeam::SetToggleDelay(Event *ev)
{
    edict->s.skinNum |= BEAM_TOGGLE;

    if (ev->NumArgs() > 2) {
        if (ev->GetBoolean(2)) {
            edict->s.skinNum |= BEAM_RANDOM_TOGGLEDELAY;
        }
        BEAM_PARM_TO_PKT(ev->GetFloat(1), edict->s.surfaces[8]);
    } else {
        BEAM_PARM_TO_PKT(ev->GetFloat(1), edict->s.surfaces[8]);
    }
}

void FuncBeam::SetSphereRadius(Event *ev)
{
    edict->s.skinNum |= BEAM_SPHERE_EFFECT;
    BEAM_PARM_TO_PKT(ev->GetFloat(1), edict->s.surfaces[7]);
}

void FuncBeam::SetNumSphereBeams(Event *ev)
{
    edict->s.skinNum |= BEAM_SPHERE_EFFECT;
    edict->s.surfaces[6] = ev->GetInteger(1);
}

void FuncBeam::SetAngle(Event *ev)
{
    Vector movedir;

    movedir = G_GetMovedir(ev->GetFloat(1));
    setAngles(movedir.toAngles());
}

void FuncBeam::SetAngles(Event *ev)
{
    setAngles(ev->GetVector(1));
}

void FuncBeam::setAngles(Vector newAngles)
{
    trace_t trace;
    Vector  endpoint;

    ScriptSlave::setAngles(newAngles);

    if (!end) {
        endpoint = origin + Vector(orientation[0]) * MAP_SIZE;

        trace = G_Trace(origin, vec_zero, vec_zero, endpoint, this, MASK_SOLID, false, "FuncBeam");

        VectorCopy(trace.endpos, edict->s.origin2);

        use_angles = true;
    }
}

void FuncBeam::SetEndPoint(const Vector& endPos)  
{
    end_point = endPos;
    VectorCopy(end_point, edict->s.origin2);
    use_angles = false;
}

void FuncBeam::SetEndPoint(Event *ev)
{
    trace_t trace;

    end_point = ev->GetVector(1);

    trace = G_Trace(origin, vec_zero, vec_zero, end_point, this, MASK_SOLID, false, "FuncBeam");
    VectorCopy(trace.endpos, edict->s.origin2);

    use_angles = false;
}

void FuncBeam::SetModel(Event *ev)
{
    setModel(ev->GetString(1));
    edict->s.renderfx &= ~RF_BEAM;
    edict->s.eType = ET_BEAM;
    edict->s.skinNum |= BEAM_USEMODEL;
}

void FuncBeam::SetDamage(Event *ev)
{
    damage = ev->GetFloat(1);
}

void FuncBeam::SetLife(Event *ev)
{
    BEAM_PARM_TO_PKT(ev->GetFloat(1), edict->s.surfaces[0]);
}

void FuncBeam::SetColor(Event *ev)
{
    Vector color = ev->GetVector(1);
    G_SetConstantLight(&edict->s.constantLight, &color[0], &color[1], &color[2], NULL, NULL);
}

void FuncBeam::SetSegments(Event *ev)
{
    edict->s.surfaces[4] = ev->GetInteger(1);
}

void FuncBeam::SetBeamShader(str beam_shader)
{
    str temp_shader;

    shader           = beam_shader;
    edict->s.tag_num = gi.imageindex(shader);

    temp_shader = shader + ".spr";
    CacheResource(temp_shader, this);
}

void FuncBeam::SetBeamShader(Event *ev)
{
    SetBeamShader(ev->GetString(1));
}

void FuncBeam::SetBeamTileShader(Event *ev)
{
    SetBeamShader(ev->GetString(1));
    edict->s.skinNum |= BEAM_TILESHADER;
}

void FuncBeam::SetMaxoffset(Event *ev)
{
    edict->s.bone_angles[0][1] = ev->GetFloat(1);
}

void FuncBeam::SetMinoffset(Event *ev)
{
    edict->s.bone_angles[0][0] = ev->GetFloat(1);
}

void FuncBeam::SetOverlap(Event *ev)
{
    BEAM_PARM_TO_PKT(ev->GetFloat(1), edict->s.surfaces[3]);
}

void FuncBeam::SetDelay(Event *ev)
{
    if (ev->NumArgs() > 2) {
        str arg = ev->GetString(1);
        if (!arg.icmp("random")) {
            edict->s.skinNum |= BEAM_RANDOM_DELAY;
        }
        BEAM_PARM_TO_PKT(ev->GetFloat(2), edict->s.surfaces[5]);
    } else {
        BEAM_PARM_TO_PKT(ev->GetFloat(1), edict->s.surfaces[5]);
    }
}

void FuncBeam::Deactivate(Event *ev)
{
    hideModel();

    CancelEventsOfType(EV_FuncBeam_Activate);
    CancelEventsOfType(EV_Activate);
    CancelEventsOfType(EV_FuncBeam_UpdateEndpoint);
    CancelEventsOfType(EV_FuncBeam_UpdateOrigin);
}

void FuncBeam::SetShootRadius(Event *ev)
{
    shootradius = ev->GetFloat(1);
}

void FuncBeam::SetPersist(Event *ev)
{
    qboolean persist = ev->GetBoolean(1);

    if (persist) {
        edict->s.skinNum |= BEAM_PERSIST_EFFECT;
    } else {
        edict->s.skinNum &= ~BEAM_PERSIST_EFFECT;
    }
}

void FuncBeam::Shoot(Event *ev)
{
    trace_t trace;
    Vector  start, endPos;

    start  = edict->s.origin;
    endPos = edict->s.origin2;

    Vector dir(endPos - start);
    Vector b1(-shootradius, -shootradius, -shootradius);
    Vector b2(shootradius, shootradius, shootradius);

    trace = G_Trace(start, b1, b2, endPos, this, MASK_SHOT, false, "FuncBeam::Activate");

    if (trace.ent && trace.ent->entity && trace.ent->entity->takedamage) {
        trace.ent->entity->Damage(this, this, damage, trace.endpos, dir, trace.plane.normal, 0, 0, MOD_BEAM);
    }

    PostEvent(EV_FuncBeam_Shoot, 0.1f);
}


void FuncBeam::Activate(Event *ev)
{
    Vector  forward;
    trace_t trace;

    showModel();

    if (end) {
        VectorCopy(end->origin, edict->s.origin2);
        PostEvent(EV_FuncBeam_UpdateEndpoint, level.frametime);
    } else if (use_angles) {
        angles.AngleVectors(&forward, NULL, NULL);

        Vector endpoint(orientation[0]);
        endpoint *= MAP_SIZE;

        if (m_bIgnoreWalls) {
            VectorCopy(endpoint, trace.endpos);
        } else {
            trace = G_Trace(origin, vec_zero, vec_zero, endpoint, this, MASK_SOLID, false, "FuncBeam::Activate");
        }

        VectorCopy(trace.endpos, edict->s.origin2);
    } else {
        if (m_bIgnoreWalls) {
            VectorCopy(end_point, trace.endpos);
        } else {
            trace = G_Trace(origin, vec_zero, vec_zero, end_point, this, MASK_SOLID, false, "FuncBeam::Activate");
        }

        VectorCopy(trace.endpos, edict->s.origin2);
    }

    if (origin_target) {
        PostEvent(EV_FuncBeam_UpdateOrigin, level.frametime);
    }

    if (damage) {
        ProcessEvent(EV_FuncBeam_Shoot);
    }

    if (life > 0 && !EventPending(EV_FuncBeam_Deactivate)) {
        PostEvent(EV_FuncBeam_Deactivate, life);
        return;
    }
}

void FuncBeam::UpdateEndpoint(Event *ev)
{
    if (end) {
        Event *ev1 = new Event(std::move(*ev));

        VectorCopy(end->origin, edict->s.origin2);
        PostEvent(ev1, level.frametime);
    }
}

void FuncBeam::UpdateOrigin(Event *ev)
{
    if (origin_target) {
        Event *ev1 = new Event(std::move(*ev));

        setOrigin(origin_target->centroid);
        PostEvent(ev1, level.frametime);
    }
}

void FuncBeam::FindEndpoint(Event *ev)
{
    if (target && strlen(target)) {
        end = (Entity *)G_FindTarget(NULL, target);
        if (end) {
            VectorCopy(end->origin, edict->s.origin2);
        }
    }
}

FuncBeam *CreateBeam(
    const char  *model,
    const char  *shader,
    const Vector& start,
    const Vector& end,
    int          numsegments,
    float        scale,
    float        life,
    float        damage,
    Entity      *origin_target
)
{
    FuncBeam *beam;
    trace_t   trace;

    beam = new FuncBeam;
    beam->setOrigin(start);

    if (origin_target) {
        beam->origin_target = origin_target;
    }

    beam->end_point = end;
    trace           = G_Trace(start, vec_zero, vec_zero, end, beam, MASK_SOLID, false, "CreateBeam");
    VectorCopy(trace.endpos, beam->edict->s.origin2);
    beam->use_angles = false;

    if (model) {
        beam->setModel(model);
        beam->edict->s.renderfx &= ~RF_BEAM;
        beam->edict->s.eType = ET_BEAM;
        beam->edict->s.skinNum |= BEAM_USEMODEL;
    }

    if (shader) {
        beam->SetBeamShader(shader);
    }

    beam->edict->s.surfaces[4] = numsegments;

    beam->setScale(scale);

    BEAM_PARM_TO_PKT(life, beam->edict->s.surfaces[0]);
    beam->life = life;

    beam->damage = damage;

    beam->ProcessEvent(EV_Activate);
    beam->PostEvent(EV_Remove, life);
    return beam;
}



void ScriptSlave::setOrigin( void )
{
	Trigger::setOrigin();
	SetNewPosition( GetLocalOrigin() );
}


