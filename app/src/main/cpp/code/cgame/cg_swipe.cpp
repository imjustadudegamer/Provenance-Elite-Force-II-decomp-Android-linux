

#include "cg_commands.h"

swipething_t        m_swipes[MAX_SWIPES];
extern int          current_entity_number;
extern refEntity_t *current_entity;
extern int          current_tiki;
extern centity_t   *current_centity;

void ClientGameCommandManager::SwipeOn(Event *ev)
{
    int       i;
    int       freeswipe = -1;
    int       instancenum;
    str       tagname_start, tagname_end;
    qhandle_t shader;

    if (ev->NumArgs() != 4) {
        return;
    }

    shader        = cgi.R_RegisterShader(ev->GetString(1));
    tagname_start = ev->GetString(2);
    tagname_end   = ev->GetString(3);

    if (current_entity_number < MAGIC_UNUSED_NUMBER) {
        instancenum = current_centity->currentState.instanceNumber;
    } else {
        instancenum = current_entity_number;
    }

    for (i = 0; i < MAX_SWIPES; i++) {
        swipething_t& swipe = m_swipes[i];

        if (!swipe.enabled
            || ((swipe.entitynum == current_entity_number) && (swipe.instancenum == instancenum)
                && (tagname_start == swipe.tagname_start) && (tagname_end == swipe.tagname_end)
                && (shader == swipe.shader))) {
            freeswipe = i;
            break;
        }
    }

    if (freeswipe == -1) {
        return;
    }

    swipething_t& swipe = m_swipes[freeswipe];

    if (!swipe.enabled) {
        swipe.Init();
        swipe.enabled     = qtrue;
        swipe.entitynum   = current_entity_number;
        swipe.instancenum = instancenum;
    }

    swipe.shader        = cgi.R_RegisterShader(ev->GetString(1));
    swipe.tagname_start = ev->GetString(2);
    swipe.tagname_end   = ev->GetString(3);
    swipe.life          = ev->GetFloat(4) * 1000.f;

    if (current_centity) {
        current_centity->clientFlags |= CF_UPDATESWIPE;
    }
}

void ClientGameCommandManager::SwipeOff(Event *ev)
{
    qboolean was_enabled;
    int      i;

    was_enabled = qfalse;
    for (i = 0; i < MAX_SWIPES; i++) {
        swipething_t& swipe = m_swipes[i];

        if (swipe.enabled && swipe.entitynum == current_entity_number) {
            swipe.enabled = qfalse;
            was_enabled   = qtrue;
        }
    }

    if (was_enabled) {
        if (current_centity) {
            current_centity->clientFlags &= ~CF_UPDATESWIPE;
        }
    }
}

void ClientGameCommandManager::Swipe(Event *ev)
{
    int           i, swipenum;
    orientation_t _or;
    int           tagnum_start, tagnum_end;
    qboolean      add_cnt_point = qfalse;
    vec3_t        tmp;

    assert(current_entity);
    assert(current_tiki);

    if (!current_entity || current_tiki < 0) {
        return;
    }

    for (swipenum = 0; swipenum < MAX_SWIPES; swipenum++) {
        swipething_t& swipe = m_swipes[swipenum];

        if (swipe.enabled && swipe.entitynum == current_entity_number) {
            while (swipe.num_live_swipes > 1) {
                i                        = (swipe.first_swipe + 1) % MAX_SWIPE_POINTS;
                swipepoint_t& swipepoint = swipe.swipepoints[i];

                if (swipepoint.time < cg.time - swipe.life) {
                    swipe.first_swipe = (swipe.first_swipe + 1) % MAX_SWIPE_POINTS;
                    swipe.num_live_swipes--;
                } else {
                    break;
                }
            }

            if (swipe.num_live_swipes == 1) {
                swipepoint_t& swipepoint = swipe.swipepoints[swipe.first_swipe];
                if (swipepoint.time < cg.time - swipe.life) {
                    swipe.num_live_swipes = 0;
                }
            }

            swipe.cntPoint.time = cg.time;
            VectorCopy(current_entity->origin, swipe.cntPoint.points[0]);

            tagnum_start = cgi.Tag_NumForName(current_tiki, swipe.tagname_start.c_str());
            cgi.TIKI_Tag_LerpOrientation(&_or, current_tiki, current_entity, tagnum_start);

            VectorClear(tmp);
            VectorClear(swipe.cntPoint.points[0]);
            VectorClear(swipe.cntPoint.points[1]);

            if (ev->NumArgs() > 0) {
                VectorCopy(ev->GetVector(1), tmp);
            } else {
                VectorCopy(current_entity->origin, tmp);
            }

            for (i = 0; i < 3; i++) {
                VectorMA(tmp, _or.origin[i], current_entity->axis[i], tmp);
            }

            VectorCopy(tmp, swipe.cntPoint.points[0]);

            tagnum_end = cgi.Tag_NumForName(current_tiki, swipe.tagname_end.c_str());
            cgi.TIKI_Tag_LerpOrientation(&_or, current_tiki, current_entity, tagnum_end);

            if (ev->NumArgs() > 0) {
                VectorCopy(ev->GetVector(1), tmp);
            } else {
                VectorCopy(current_entity->origin, tmp);
            }

            for (i = 0; i < 3; i++) {
                VectorMA(tmp, _or.origin[i], current_entity->axis[i], tmp);
            }

            VectorCopy(tmp, swipe.cntPoint.points[1]);

            if (swipe.num_live_swipes == 0) {
                add_cnt_point = qtrue;
            } else {
                float         deltime = swipe.life / float(MAX_SWIPES);
                swipepoint_t& lastpoint =
                    swipe.swipepoints[(swipe.first_swipe + swipe.num_live_swipes - 1) % MAX_SWIPE_POINTS];

                if (swipe.cntPoint.time - lastpoint.time >= deltime) {
                    add_cnt_point = qtrue;
                }
            }

            cgi.R_SwipeBegin(cg.time, swipe.life, swipe.shader);

            if (add_cnt_point) {
                swipepoint_t& swipepoint =
                    swipe.swipepoints[(swipe.first_swipe + swipe.num_live_swipes) % MAX_SWIPE_POINTS];

                swipepoint = swipe.cntPoint;

                if (swipe.num_live_swipes == MAX_SWIPE_POINTS) {
                    swipe.first_swipe = (swipe.first_swipe + 1) % MAX_SWIPE_POINTS;
                } else {
                    swipe.num_live_swipes++;
                }
            }

            for (i = 0; i != swipe.num_live_swipes; i++) {
                swipepoint_t& swipepoint = swipe.swipepoints[(swipe.first_swipe + i) % MAX_SWIPE_POINTS];

                cgi.R_SwipePoint(swipepoint.points[0], swipepoint.points[1], swipepoint.time);
            }

            if (!add_cnt_point) {
                cgi.R_SwipePoint(swipe.cntPoint.points[0], swipe.cntPoint.points[1], swipe.cntPoint.time);
            }

            cgi.R_SwipeEnd();
        }
    }
}

void ClientGameCommandManager::ClearSwipes(void)
{
    int i;

    for (i = 0; i < MAX_SWIPES; i++) {
        swipething_t& swipe = m_swipes[i];

        swipe.enabled = qfalse;
    }

    cgi.R_SwipeBegin(0.f, 0.f, -1);
    cgi.R_SwipeEnd();
}

void CG_ClearSwipes(void)
{
    commandManager.ClearSwipes();
}

void CG_UpdateSwipes(void)
{
    int i;

    for (i = 0; i < MAX_SWIPES; i++) {
        swipething_t& swipe = m_swipes[i];

        if (!swipe.enabled) {
            continue;
        }

        if (swipe.entitynum >= MAGIC_UNUSED_NUMBER) {
            continue;
        }

        if (swipe.entitynum < 0 || swipe.entitynum >= MAX_GENTITIES) {
            swipe.enabled = qfalse;
            continue;
        }

        centity_t& cent = cg_entities[swipe.entitynum];

        if (!cent.currentValid || swipe.instancenum != cent.currentState.instanceNumber) {
            swipe.enabled = qfalse;
        }
    }
}
