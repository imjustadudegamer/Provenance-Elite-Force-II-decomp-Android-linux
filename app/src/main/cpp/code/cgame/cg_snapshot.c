

#include "cg_local.h"
#define R120_PROBE_GET(name, def, flags) cgi.Cvar_Get(name, def, flags)
#define R120_PROBE_PRINT                 cgi.Printf
#include "../qcommon/qa_r120.h"

static void CG_ResetEntity(centity_t *cent)
{
    int      tiki;

    VectorCopy(cent->currentState.origin, cent->lerpOrigin);

    VectorCopy(cent->currentState.angles, cent->lerpAngles);

    if (cent->teleported) {
        cent->teleported = qfalse;
        return;
    }

    cent->client_color[0] = 1;
    cent->client_color[1] = 1;
    cent->client_color[2] = 1;
    cent->client_color[3] = 1;

    cent->tikiLoopSound = 0;

    cent->clientFlags = 0;

    cent->splash_last_spawn_time = 0;
    cent->splash_still_count     = -1;

    // `itemringshader' -- retail does not clear it here, but retail also never reuses a
    cent->itemRingShader = 0;

    //   R_Model_GetHandle returns -1 too but re-resolves a model retail never reads).
    tiki = cgs.model_tiki[cent->currentState.modelindex];
    CG_RemoveClientEntity(cent->currentState.number, tiki, cent);

    if (cent->currentState.eType < 3) {
        CG_ClearModelAnimation(
            tiki, &cent->animstate[0], cent->currentState.anim, cg.snap->serverTime, cent->lerpOrigin,
            cent->currentState.number, cent->currentState.animationRate
        );
        CG_ClearModelAnimation(
            tiki, &cent->animstate[1], cent->currentState.torso_anim, cg.snap->serverTime, cent->lerpOrigin,
            cent->currentState.number, cent->currentState.animationRate
        );
    }

    cent->bFootOnGround_Left  = qtrue;
    cent->bFootOnGround_Right = qtrue;

    if (cent->currentState.eType == ET_PLAYER) {
        CG_ResetPlayerEntity(cent);
    }
}

static void CG_TransitionEntity(centity_t *cent)
{
    cent->currentState = cent->nextState;
    cent->currentValid = qtrue;

    if (!cent->interpolate) {
        CG_ResetEntity(cent);
    }

    cent->interpolate = qfalse;
    cent->teleported = qfalse;

    if (cent->currentState.eType == ET_EVENTS) {
        CG_Event(cent);
    }
}

void CG_SetInitialSnapshot(snapshot_t *snap)
{
    int            i;
    centity_t     *cent;
    entityState_t *state;

    cg.snap = snap;

    R131M1_PROBE( "init", "snapFlags=%d snapTime=%d cmdSeq=%d cgTime=%d latestNum=%d processed=%d",
        snap->snapFlags, snap->serverTime, snap->serverCommandSequence, cg.time,
        cg.latestSnapshotNum, cgs.processedSnapshotNum );

    CG_BuildSolidList();

    CG_ExecuteNewServerCommands(snap->serverCommandSequence);

    for (i = 0; i < cg.snap->numEntities; i++) {
        state = &cg.snap->entities[i];
        cent  = &cg_entities[state->number];

        cent->currentState = *state;
        cent->interpolate  = qfalse;
        cent->currentValid = qtrue;

        // entityState_t.quat is declared "not sent over" (SDK q_shared.h:1774): it is
        EulerToQuat(cent->currentState.angles, cent->currentState.quat);

        CG_ResetEntity(cent);
    }

    cgi.MUSIC_UpdateMood(snap->ps.current_music_mood, snap->ps.fallback_music_mood);
    cgi.MUSIC_UpdateVolume(snap->ps.music_volume, snap->ps.music_volume_fade_time);
    cgi.MUSIC_UpdateDucking(snap->ps.allowMusicDucking);
    cgi.S_SetReverb(snap->ps.reverb_type, snap->ps.reverb_level);

    for (i = 0; i < snap->number_of_sounds; i++) {
        CG_ProcessSound(&snap->sounds[i]);
    }
}

static void CG_TransitionSnapshot(void)
{
    centity_t  *cent;
    snapshot_t *oldFrame;
    qboolean    restarted;
    int         i;

    CG_ExecuteNewServerCommands(cg.nextSnap->serverCommandSequence);

    for (i = 0; i < cg.snap->numEntities; i++) {
        cent               = &cg_entities[cg.snap->entities[i].number];
        cent->currentValid = qfalse;
    }

    oldFrame = cg.snap;
    cg.snap  = cg.nextSnap;


    restarted = ((oldFrame->snapFlags ^ cg.snap->snapFlags) & SNAPFLAG_SERVERCOUNT) != 0;
    if (restarted) {
        CG_ServerRestarted();
    }

    for (i = 0; i < cg.snap->numEntities; i++) {
        cent = &cg_entities[cg.snap->entities[i].number];
        if (restarted) {
            cent->interpolate = qfalse;
            cent->teleported  = qfalse;
        }
        CG_TransitionEntity(cent);
    }

    for (i = 0; i < cg.snap->number_of_sounds; i++) {
        CG_ProcessSound(&cg.snap->sounds[i]);
    }

    {
        const char *dialogSound = "";
        const char *dialogTextSound = "";

        if (cg.snap->ps.dialogSoundIndex >= 0) {
            dialogSound = CG_ConfigString(CS_SOUNDS + cg.snap->ps.dialogSoundIndex);
        }
        if (cg.snap->ps.dialogTextSoundIndex >= 0) {
            dialogTextSound = CG_ConfigString(CS_SOUNDS + cg.snap->ps.dialogTextSoundIndex);
        }


        cgi.UI_DisplayDialogConsole(dialogSound, dialogTextSound, cg.snap->ps.dialogEntnum);
    }

    cg.nextSnap = NULL;


    if (oldFrame) {
        playerState_t *ops, *ps;

        ops = &oldFrame->ps;
        ps  = &cg.snap->ps;

        if (ps->pm_flags & PMF_RESPAWNED) {
            cg.thisFrameTeleport = qtrue;
        }

        cgi.MUSIC_UpdateMood(ps->current_music_mood, ps->fallback_music_mood);

        if ((ops->music_volume != ps->music_volume) || (ops->music_volume_fade_time != ps->music_volume_fade_time)) {
            cgi.MUSIC_UpdateVolume(ps->music_volume, ps->music_volume_fade_time);
        }

        if (ops->allowMusicDucking != ps->allowMusicDucking) {
            cgi.MUSIC_UpdateDucking(ps->allowMusicDucking);
        }

        if ((ops->reverb_type != ps->reverb_type) || (ops->reverb_level != ps->reverb_level)) {
            cgi.S_SetReverb(ps->reverb_type, ps->reverb_level);
        }

        if (cg.demoPlayback || (cg.snap->ps.pm_flags & PMF_NO_PREDICTION) || cg_nopredict->integer
            || cg_synchronousClients->integer) {
            CG_TransitionPlayerState(ps, ops);
        }
    }

}

static void CG_SetNextSnap(snapshot_t *snap)
{
    int            num;
    entityState_t *es;
    centity_t     *cent;

    cg.nextSnap = snap;

    cg_entities[cg.snap->ps.clientNum].interpolate = qtrue;

    for (num = 0; num < snap->numEntities; num++) {
        es   = &snap->entities[num];
        cent = &cg_entities[es->number];

        cent->nextState = *es;

        EulerToQuat(cent->nextState.angles, cent->nextState.quat);

        if (!cent->currentValid || ((cent->currentState.eFlags ^ es->eFlags) & EF_TELEPORT_BIT)
            || (cent->currentState.parent != es->parent) || (cent->currentState.modelindex != es->modelindex)) {
            cent->interpolate = qfalse;
            if (cent->currentValid) {
                cent->teleported = qtrue;
            }
        } else {
            cent->interpolate = qtrue;
        }
    }

    if (cg.snap && (snap->ps.pm_flags & PMF_RESPAWNED)) {
        cg.nextFrameTeleport = qtrue;
    } else {
        cg.nextFrameTeleport = qfalse;
    }

    if (cg.nextSnap->ps.clientNum != cg.snap->ps.clientNum) {
        cg.nextFrameTeleport = qtrue;
    }

    if ((cg.nextSnap->ps.camera_flags & CF_CAMERA_CUT_BIT) != (cg.snap->ps.camera_flags & CF_CAMERA_CUT_BIT)) {
        cg.nextFrameCameraCut = qtrue;
    } else {
        cg.nextFrameCameraCut = qfalse;
    }

    if ((cg.nextSnap->snapFlags ^ cg.snap->snapFlags) & SNAPFLAG_SERVERCOUNT) {
        cg.nextFrameTeleport = qtrue;
        cg.viewInterpTime    = -1;
    }

    VectorCopy(cg.nextSnap->ps.damage_angles, cg.snapDamageAngles);

    CG_BuildSolidList();
}

static snapshot_t *CG_ReadNextSnapshot(void)
{
    qboolean    r;
    snapshot_t *dest;

    if (cg.latestSnapshotNum > cgs.processedSnapshotNum + 1000) {
        cgi.Printf(
            "WARNING: CG_ReadNextSnapshot: way out of range, %i > %i\n",
            cg.latestSnapshotNum,
            cgs.processedSnapshotNum
        );
    }

    while (cgs.processedSnapshotNum < cg.latestSnapshotNum) {
        if (cg.snap == &cg.activeSnapshots[0]) {
            dest = &cg.activeSnapshots[1];
        } else {
            dest = &cg.activeSnapshots[0];
        }

        cgs.processedSnapshotNum++;
        r = cgi.GetSnapshot(cgs.processedSnapshotNum, dest);

        if (r) {
            CG_AddLagometerSnapshotInfo(dest);
            return dest;
        }


        CG_AddLagometerSnapshotInfo(NULL);

    }

    return NULL;
}

void CG_ProcessSnapshots(int serverTime)
{
    snapshot_t *snap;
    int         n;

    (void)serverTime;

    cgi.GetCurrentSnapshotNumber(&n, &cg.latestSnapshotTime);
    if (n != cg.latestSnapshotNum) {
        if (n < cg.latestSnapshotNum) {
            cgi.Error(ERR_DROP, "CG_ProcessSnapshots: n < cg.latestSnapshotNum");
        }
        cg.latestSnapshotNum = n;
    }

    while (!cg.snap) {
        snap = CG_ReadNextSnapshot();
        if (!snap) {
            return;
        }

        if (!(snap->snapFlags & SNAPFLAG_NOT_ACTIVE)) {
            CG_SetInitialSnapshot(snap);
        }
    }

    do {
        if (!cg.nextSnap) {
            snap = CG_ReadNextSnapshot();

            if (!snap) {
                break;
            }

            CG_SetNextSnap(snap);

            if (cg.nextSnap->serverTime < cg.snap->serverTime) {
                if (!((cg.nextSnap->snapFlags ^ cg.snap->snapFlags) & SNAPFLAG_SERVERCOUNT)) {
                    cgi.Error(ERR_DROP, "CG_ProcessSnapshots: Server time went backwards");
                }
            }
        }

        if (cg.time >= cg.snap->serverTime && cg.time < cg.nextSnap->serverTime) {
            break;
        }

        CG_TransitionSnapshot();
    } while (1);

    if (cg.snap == NULL) {
        cgi.Error(ERR_DROP, "CG_ProcessSnapshots: cg.snap == NULL");
    }
    if (cg.time < cg.snap->serverTime) {
        cg.time = cg.snap->serverTime;
    }
    if (cg.nextSnap != NULL && cg.nextSnap->serverTime <= cg.time) {
        cgi.Error(ERR_DROP, "CG_ProcessSnapshots: cg.nextSnap->serverTime <= cg.time");
    }
}
