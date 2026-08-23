#!/usr/bin/env python3
"""R132-N2 -- complete per-field decoder of the retail entity restore stream.

Extends docs/scratch_w2/ent_attach_scan.py (which constraint-fit only
parent/tag/modelindex/attach_offset) into a FULL field-by-field decode of

    Entity::Archive   (SDK dlls/game/entity.cpp:6521-6861)
    G_ArchiveEdict    (SDK dlls/game/g_utils.cpp:1313-1497)
    Sentient::Archive (SDK dlls/game/sentient.cpp:3154-3288)
    Actor::Archive    (SDK dlls/game/actor.cpp:15267-...  )

for every entity in a retail .sav, so the port's post-restore live state can be
diffed field by field against retail's own bytes.

The walk is transactional per entity: if a class body cannot be modelled the
whole entity falls back to its declared size prefix (archive.cpp:897-908) and
the failure is recorded, so a bad model can never desynchronise the stream.

READ-ONLY on the save; prints tables.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sav_stream as S

_SAVARG = [a for a in sys.argv[1:] if not a.startswith("-") and a != "all"]
SAV = _SAVARG[0] if _SAVARG else (
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + '/personal/oracle/saves/retail_corpus_116/'
    'Elite Force II/base/save/Jens-Christian-Huus/SaveGame1057164666/'
    'SaveGame1057164666.sav')

u32 = lambda b, o: struct.unpack_from('<I', b, o)[0]
i32 = lambda b, o: struct.unpack_from('<i', b, o)[0]
f32 = lambda b, o: struct.unpack_from('<f', b, o)[0]

# ---------------------------------------------------------------- constants
NUM_BONE_CONTROLLERS = 5          # q_shared.h:1683
NUM_MORPH_CONTROLLERS = 10        # q_shared.h:1687
NUM_EFFECTS_ANIMS = 4             # q_shared.h:1689
MAX_MODEL_SURFACES = 32           # q_shared.h:1667
SZ_ORIENTATION = 36               # entity.h:423 float[3][3]
SZ_CPLANE = 20                    # q_shared.h:880-886
SZ_GCLIENT = 3744                 # see G_ArchiveEdict note in the port
SZ_MORPH = NUM_MORPH_CONTROLLERS * 8   # morph_controller_t {int,float}
SZ_SURFACES = MAX_MODEL_SURFACES  # byte[32]
SZ_EFFECTSANIMS = NUM_EFFECTS_ANIMS * 4
SZ_MAT = 36                       # float[3][3]
SZ_QUAT = 16

# SDK bg_public.h:489-510
ET_NAMES = {
    0: 'ET_MODELANIM', 1: 'ET_PLAYER', 2: 'ET_ITEM', 3: 'ET_GENERAL',
    4: 'ET_MISSILE', 5: 'ET_MOVER', 6: 'ET_BEAM', 7: 'ET_MULTIBEAM',
    8: 'ET_SPRITE', 9: 'ET_PORTAL', 10: 'ET_EVENT_ONLY', 11: 'ET_RAIN',
    12: 'ET_LEAF', 13: 'ET_SPEAKER', 14: 'ET_PUSH_TRIGGER',
    15: 'ET_TELEPORT_TRIGGER', 16: 'ET_DECAL', 17: 'ET_EMITTER',
    18: 'ET_ROPE', 19: 'ET_EVENTS',
}
SOLID_NAMES = {0: 'SOLID_NOT', 1: 'SOLID_TRIGGER', 2: 'SOLID_BBOX', 3: 'SOLID_BSP'}
DAMAGE_NAMES = {0: 'DAMAGE_NO', 1: 'DAMAGE_YES', 2: 'DAMAGE_AIM'}
# SDK g_phys.h:39-54
MOVE_NAMES = {
    0: 'MOVETYPE_NONE', 1: 'MOVETYPE_STATIONARY', 2: 'MOVETYPE_NOCLIP',
    3: 'MOVETYPE_PUSH', 4: 'MOVETYPE_STOP', 5: 'MOVETYPE_WALK',
    6: 'MOVETYPE_STEP', 7: 'MOVETYPE_FLY', 8: 'MOVETYPE_TOSS',
    9: 'MOVETYPE_FLYMISSILE', 10: 'MOVETYPE_BOUNCE', 11: 'MOVETYPE_SLIDE',
    12: 'MOVETYPE_ROPE', 13: 'MOVETYPE_GIB', 14: 'MOVETYPE_VEHICLE',
}
# SDK entity.h:75-94
FL_NAMES = [
    (1 << 0, 'FL_FLY'), (1 << 1, 'FL_SWIM'), (1 << 2, 'FL_INWATER'),
    (1 << 3, 'FL_GODMODE'), (1 << 4, 'FL_NOTARGET'), (1 << 5, 'FL_PARTIALGROUND'),
    (1 << 6, 'FL_TEAMSLAVE'), (1 << 7, 'FL_NO_KNOCKBACK'), (1 << 8, 'FL_THINK'),
    (1 << 9, 'FL_BLOOD'), (1 << 10, 'FL_DIE_GIBS'), (1 << 11, 'FL_DIE_EXPLODE'),
    (1 << 12, 'FL_ROTATEDBOUNDS'), (1 << 13, 'FL_DONTSAVE'), (1 << 14, 'FL_IMMOBILE'),
    (1 << 15, 'FL_PARTIAL_IMMOBILE'), (1 << 16, 'FL_STUNNED'), (1 << 17, 'FL_POSTTHINK'),
    (1 << 18, 'FL_TOUCH_TRIGGERS'), (1 << 19, 'FL_AUTOAIM'),
]

# DamageModification.hpp:34-43
DMOD_NAMES = {0: 'UNDEFINED', 1: 'TIKI_NAME', 2: 'NAME', 3: 'GROUP',
              4: 'ACTOR_TYPE', 5: 'TARGETNAME', 6: 'DAMAGE_TYPE'}

# Actor flags -- SDK actorincludes.h enum (103 entries, MAX 102)
ACTOR_FLAG_NAMES = [
    'PM_ENEMY', 'PM_FRIEND', 'PM_NEUTRAL', 'PM_CIVILIAN', 'PM_ANY',
    'ENEMY_IN_RANGE', 'ENEMY_TOO_CLOSE', 'ENEMY_ABOUT_TO_SHOOT', 'ALERT',
    'ANIM_DONE', 'ANIM_IN_TORSO', 'ANIM_IN_LEGS', 'ANIM_GETOUT', 'CAN_SHOOT',
    'CAN_DUCK', 'CAN_COVER', 'CAN_FALL', 'CAN_RELOAD', 'CAN_RUN_TO_COVER',
    'CAN_WALK_TO_COVER', 'CAN_ADVANCE', 'CAN_POSITION', 'CAN_STAND',
    'CAN_CROUCH', 'CAN_PRONE', 'CAN_MELEE_ATTACK', 'CAN_USE_STAIRS',
    'CAN_LOWER_LADDER', 'CAN_JUMP_UP_LEDGES', 'CAN_LADDER_UP', 'CAN_LADDER_DOWN',
    'ENEMY_IS_PLAYER', 'ENEMY_IS_ACTOR', 'ENEMY_IS_VISIBLE', 'ENEMY_OFFHAND',
    'ENEMY_IN_TRIG', 'HARD_ENTRANCE', 'JUST_HIT_WALL', 'FOUND_COVER',
    'COVER_CHANGED', 'IMMORTAL', 'PATH_VALID', 'GOAL_REACHED', 'DIE_COMPLETELY',
    'CORNER_PATH', 'COVER_PATH', 'DESTINATION_REACHED', 'ALLOWED_TO_KILL',
    'GREETING', 'NO_LOOKAROUND', 'NO_IDLE_THREAD', 'NO_ENEMY_CHATTER',
    'NO_ALERT_TALK', 'NO_PAIN', 'NO_PAIN_ANIM', 'NO_DEATH_ANIM', 'NO_ALERT_ANIM',
    'NO_TIME_WAIT', 'FAKE_ENEMY', 'NEW_ENEMY', 'NO surprise', 'AIM_AT_ENEMY',
    'ALLOW_MOUNT', 'CAN_WALK', 'CAN_RUN', 'CAN_SPRINT', 'CAN_COVER_FIRE',
    'CHANGE_ANIM', 'DISABLED', 'IN_ALCOVE', 'SHOULD_DO_ALCOVE', 'CAN_SHOOT_FRIEND',
    'CLEAN_ARMS', 'HAVE_KILL_TARGET', 'NO_CAN_DETECT', 'MOVED_TO_SPOT',
    'HAS_GUARDBEHAVIOR', 'CINEMATIC', 'PLAY_HURT_SOUND', 'DONT_POSTTHINK',
    'POST_DEATH_COLLISION', 'MUST_SPEAK', 'NO_MG', 'NO_CHATTER', 'DONT_SLEEP',
    'NO_TALK', 'SIMPLE_DIALOG', 'FORCED_IDLE_ANIM', 'NO_WEAPON', 'NO_HANDS',
    'DONT_REACT_TO_PURSUE', 'MULTIPLE_ATTACHMENTS', 'USE_ALL_ANIMS',
    'NO_TALK_RESPONSE', 'FAILED_JOB', 'IS_TURRET', 'CANNOT_BE_HATED',
    'CAN_CHANGE_ANIM', 'NO_PAIN_HANDLER', 'DONT_CHANGE_AI', 'DO_NOT_TARGET',
    'ATTACK_TAKES_PRECEDENCE', 'ALLOW_ACTORWAREVENT', 'MAX_ACTOR_FLAGS',
]


class Ent(object):
    __slots__ = ('entnum', 'classname', 'body', 'size', 'fields', 'error',
                 'npc', 'flags_words')


def vec(buf, p):
    return struct.unpack_from('<3f', buf, p)


def vec4(buf, p):
    return struct.unpack_from('<4f', buf, p)


def fstr(buf, p):
    n = u32(buf, p)
    return buf[p + 4:p + 4 + n].decode('latin1', 'replace'), p + 4 + n


def decode_entity(buf, ent, trace=False):
    """Walk one entity object body through Entity::Archive + G_ArchiveEdict."""
    def event_or_stop(pnow):
        try:
            return read_event_bytes(buf, pnow, ent.body + ent.size)
        except (S.StreamError, struct.error):
            raise EventUnmodelled('archived Event body not modelled at body+0x%x'
                                  % (pnow - ent.body))
    F = {}
    p = ent.body
    end = ent.body + ent.size

    def need(n):
        if n < 0:
            raise StreamAbort('negative count %d at body+0x%x' % (n, p - ent.body))
        if p + n > end:
            raise StreamAbort('need %d at body+0x%x' % (n, p - ent.body))

    def v3(name):
        nonlocal p
        need(12)
        F[name] = vec(buf, p)
        p += 12

    def v4(name):
        nonlocal p
        need(16)
        F[name] = vec4(buf, p)
        p += 16

    def i(name):
        nonlocal p
        F['_cur'] = name
        need(4)
        F[name] = i32(buf, p)
        p += 4

    def u(name):
        nonlocal p
        F['_cur'] = name
        need(4)
        F[name] = u32(buf, p)
        p += 4

    def f(name):
        nonlocal p
        F['_cur'] = name
        need(4)
        F[name] = f32(buf, p)
        p += 4

    def raw(name, n):
        nonlocal p
        F['_cur'] = name
        need(n)
        F[name] = buf[p:p + n]
        p += n

    def s(name):
        nonlocal p
        F['_cur'] = name
        need(4)
        val, p2 = fstr(buf, p)
        if p2 > end:
            raise StreamAbort('string overrun in %s' % name)
        F[name] = val
        p = p2

    def b1(name):
        nonlocal p
        need(1)
        F[name] = buf[p]
        p += 1

    try:
        # ------------------------------------------------ Entity::Archive head
        v3('_localOrigin')
        i('_lastTouchedList.count')
        n = F['_lastTouchedList.count']
        need(4 * n)
        F['_lastTouchedList.ptrs'] = [i32(buf, p + 4 * k) for k in range(n)]
        p += 4 * n
        b1('_fulltrace')
        i('_groupID')

        # ------------------------------------------------------- G_ArchiveEdict
        # The gclient_t raw block is conditional on edict->client, which the
        # stream does not flag.  Detect it: the entity prefix before
        # s.instanceNumber is 21 + 4*n bytes (n = _lastTouchedList count), so
        # s.eType sits at body+25+4n and must be a legal eType.
        if F.get('_isclient') is None:
            probe = ent.body + 25 + 4 * F['_lastTouchedList.count']
            et = i32(buf, probe) if probe + 4 <= end else -1
            F['_isclient'] = not (0 <= et < 20)
        if F['_isclient']:
            raw('client.raw', SZ_GCLIENT)
        i('s.instanceNumber')
        i('s.eType')
        i('s.eFlags')
        v3('s.netorigin')
        v3('s.origin')
        v3('s.origin2')
        v3('s.netangles')
        v3('s.angles')
        v3('s.viewangles')
        u('s.constantLight')
        s('s.loopSound_name')
        f('s.loopSoundVolume')
        f('s.loopSoundMinDist')
        i('s.parent')
        i('s.tag_num')
        i('s.attach_use_angles')
        v3('s.attach_offset')
        v3('s.attach_angles_offset')
        i('s.modelindex')
        i('s.viewmodelindex')
        i('s.skinNum')
        s('s.customShader_name')
        s('s.customEmitter_name')
        f('s.animationRate')
        i('s.anim')
        i('s.frame')
        i('s.crossblend_time')
        i('s.torso_anim')
        i('s.torso_frame')
        i('s.torso_crossblend_time')
        F['s.bone_tag'] = []
        F['s.bone_angles'] = []
        F['s.bone_quat'] = []
        for k in range(NUM_BONE_CONTROLLERS):
            i('s.bone_tag[%d]' % k)
            F['s.bone_tag'].append(F.pop('s.bone_tag[%d]' % k))
            v3('s.bone_angles[%d]' % k)
            F['s.bone_angles'].append(F.pop('s.bone_angles[%d]' % k))
            v4('s.bone_quat[%d]' % k)
            F['s.bone_quat'].append(F.pop('s.bone_quat[%d]' % k))
        raw('s.morph_controllers', SZ_MORPH)
        raw('s.surfaces', SZ_SURFACES)
        i('s.clientNum')
        i('s.groundEntityNum')
        i('s.solid')
        f('s.scale')
        f('s.alpha')
        i('s.renderfx')
        u('s.affectingViewModes')
        i('s.archeTypeIndex')
        i('s.missionObjective')
        i('s.infoIcon')
        raw('s.effectsAnims', SZ_EFFECTSANIMS)
        i('s.bindparent')
        v4('s.quat')
        raw('s.mat', SZ_MAT)
        i('svflags')
        v3('edict.mins')
        v3('edict.maxs')
        i('edict.contents')
        v3('edict.absmin')
        v3('edict.absmax')
        f('edict.radius')
        v3('edict.centroid')
        v3('edict.currentOrigin')
        v3('edict.currentAngles')
        i('edict.ownerNum')
        i('edict.solid_enum')
        f('edict.freetime')
        f('edict.spawntime')
        s('edict.entname')
        i('edict.clipmask')

        # -------------------------------------------- Entity::Archive continuation
        s('_archetype')
        b1('_missionObjective')
        s('_targetPos')
        b1('_networkDetail')
        i('spawnflags')
        s('model')
        for nm in ('total_delta', 'mins', 'maxs', 'absmin', 'absmax', 'centroid',
                   'velocity', 'avelocity', 'origin', 'angles', 'size'):
            v3(nm)
        i('movetype')
        i('mass')
        f('gravity')
        raw('orientation', SZ_ORIENTATION)
        v3('localangles')
        i('groundentity_idx')
        raw('groundplane', SZ_CPLANE)
        i('groundcontents')
        i('numsurfaces')
        f('lightRadius')
        s('target')
        s('targetname')
        s('killtarget')
        f('health')
        f('max_health')
        i('deadflag')
        i('flags')
        i('watertype')
        i('waterlevel')
        i('takedamage')
        i('damage_type')
        need(4)
        F['look_at_me'] = i32(buf, p)
        p += 4
        b1('projectilesCanStickToMe')
        s('explosionModel')
        # entityVars: ScriptVariableList -> Container<ScriptVariable>
        i('entityVars.count')
        n = F['entityVars.count']
        vars_ = []
        for _ in range(n):
            s('var.name.tmp')
            vn = F.pop('var.name.tmp')
            # ScriptVariable::Archive -- type word + payload
            t = i32(buf, p)
            p += 4
            if t == 0:            # none
                payload = None
            elif t == 1:          # int
                payload = i32(buf, p); p += 4
            elif t == 2:          # float
                payload = f32(buf, p); p += 4
            elif t == 3:          # string
                payload, p = fstr(buf, p)
            elif t == 4:          # vector
                payload = vec(buf, p); p += 12
            else:
                payload = '<type %d>' % t
            vars_.append((vn, t, payload))
        F['entityVars'] = vars_
        u('_affectingViewModes')
        v3('watch_offset')
        # 4 pluggable presence flags (animate/mover/bind_info/morph_info).
        # ⚠️ These are arc.ArchiveBoolean == qboolean == FOUR bytes each, not
        # ArchiveBool; the ObjectProgram flag below is ArchiveBoolean too.
        for nm in ('animate', 'mover'):
            i(nm + '.present')
            if F[nm + '.present']:
                # arc.ArchiveObject(animate) -- a full tagged frame; skip it by
                # its own size prefix, we do not need its contents.
                nmcls, _b, _s, _e = skip_object(buf, p, end)
                F[nm + '.class'] = nmcls
                p = _e
        for nm in ('bind_info', 'morph_info'):
            i(nm + '.present')
            if not F[nm + '.present']:
                continue
            if nm == 'bind_info':
                # BindInfo::Archive -- SDK entity.h:284-300, inline
                i('bind_info.numchildren')
                raw('bind_info.children', 8 * 4)
                s('bind_info.moveteam')
                i('bind_info.teamchain')
                i('bind_info.teammaster')
                i('bind_info.bindmaster')
                i('bind_info.bind_use_my_angles')
                i('bind_info.detach_at_death')
            else:
                # MorphInfo::Archive -- SDK entity.h:336-352, inline
                raw('morph_info.controllers', 10 * 24)
                i('morph_info.controller_on')
        i('ObjectProgram.present')
        if F['ObjectProgram.present']:
            raise StopWalk('ObjectProgram body not modelled')
        b1('damageModSystem.present')
        if F['damageModSystem.present']:
            i('damageModSystem.numobj')
            dmods = []
            for _ in range(F['damageModSystem.numobj']):
                i('dmod.type')
                t = F.pop('dmod.type')
                # DamageModifier::Archive -- DamageModification.hpp:151-161
                f('dmod.multiplier')
                f('dmod.chance')
                f('dmod.painBaseLine')
                # the subclass tail -- DamageModification.hpp:194-... :
                #   TIKI_NAME/NAME/TARGETNAME -> string
                #   GROUP/ACTOR_TYPE/DAMAGE_TYPE -> int
                extra = None
                if t in (1, 2, 5):
                    extra, p = fstr(buf, p)
                else:
                    need(4)
                    extra = i32(buf, p)
                    p += 4
                dmods.append((DMOD_NAMES.get(t, t), round(F.pop('dmod.multiplier'), 4),
                              round(F.pop('dmod.chance'), 4),
                              round(F.pop('dmod.painBaseLine'), 4), extra))
            F['damageModSystem'] = dmods
            if os.environ.get('N2_DEBUG'):
                print('    # dmods=%r at body+0x%x' % (dmods, p - ent.body))
        b1('useData.present')
        if F['useData.present']:
            raise StopWalk('useData body not modelled')

        # ====================================================== Sentient::Archive
        # SDK sentient.cpp:3154-3288, verbatim order.
        f('_criticalHealthPercentage')
        for k in ('_damageThreshold.duration', '_damageThreshold.startTime',
                  '_damageThreshold.maxDamage', '_damageThreshold.currentDamage',
                  'shotsFiredThisVolley', '_hateModifier'):
            f(k)
        i('inventory.count')
        need(4 * F['inventory.count'])
        F['inventory'] = [i32(buf, p + 4 * k) for k in range(F['inventory.count'])]
        p += 4 * F['inventory.count']
        i('ammo_inventory.count')
        F['ammo_inventory.classes'] = []
        for _ in range(F['ammo_inventory.count']):
            F['ammo_inventory.classes'].append(skip_object(buf, p, end))
            p = F['ammo_inventory.classes'][-1][3]
        i('newWeapon.ptr')
        i('currentBaseArmor.ptr')
        v3('offset_color')
        v3('offset_delta')
        f('offset_time')
        f('knock_start_time')
        s('blood_model')
        i('last_surface_hit')
        i('last_bone_hit')
        u('last_region_hit')
        i('immunities.count')
        need(4 * F['immunities.count'])
        F['immunities'] = [i32(buf, p + 4 * k) for k in range(F['immunities.count'])]
        p += 4 * F['immunities.count']
        i('resistances.count')
        F['resistances'] = []
        for _ in range(F['resistances.count']):
            i('res.MODIndex')
            a_ = F.pop('res.MODIndex')
            i('res.Amount')
            F['resistances'].append((a_, F.pop('res.Amount')))
        # MAX_ACTIVE_WEAPONS == NUM_ACTIVE_WEAPONS == WEAPON_ANY == 3
        # (g_utils.h:286-294) -- a compile-time constant, NOT a stream count.
        F['activeWeaponList.ptrs'] = []
        for _k in range(3):
            i('activeWeaponList[%d]' % _k)
            F['activeWeaponList.ptrs'].append(F.pop('activeWeaponList[%d]' % _k))
        u('_viewMode')
        b1('_canSendInjuredEvent')
        b1('_headWatchAllowed')
        b1('_displayFireEffect')
        v3('gunoffset')
        v3('eyeposition')
        i('viewheight')
        i('means_of_death')
        for k in ('in_melee_attack', 'in_ranged_attack', 'in_block', 'in_stun'):
            i(k)          # ArchiveBoolean == qboolean == 4 bytes
        i('on_fire')
        f('on_fire_stop_time')
        f('next_catch_on_fire_time')
        F['on_fire_tagnums'] = []
        for k in range(3):
            i('on_fire_tagnums[%d]' % k)
            F['on_fire_tagnums'].append(F.pop('on_fire_tagnums[%d]' % k))
        i('fire_owner.ptr')
        i('attack_blocked')
        f('attack_blocked_time')
        f('max_mouth_angle')
        i('max_gibs')
        f('next_bleed_time')

        # ========================================================= Actor::Archive
        # SDK actor.cpp:15267-... up to and including the six flag words.
        i('forcedEnemy.ptr')
        s('newanim')
        i('newanimnum')
        i('animnum')
        s('animname')
        F['newanimevent.valid'] = i32(buf, p)
        p += 4
        if F['newanimevent.valid'] == 0:
            p = event_or_stop(p)
        s('last_anim_event_name')
        s('newTorsoAnim')
        i('newTorsoAnimNum')
        s('TorsoAnimName')
        F['newTorsoAnimEvent.valid'] = i32(buf, p)
        p += 4
        if F['newTorsoAnimEvent.valid'] == 0:
            p = event_or_stop(p)
        s('last_torso_anim_event_name')
        for k in ('absoluteMin', 'absoluteMax', 'preferredMin', 'preferredMax',
                  'activationDelay', 'activationStart'):
            f(k)
        i('actortype')
        i('targetType')
        i('validTarget')
        b1('_checkedChance')
        b1('_levelAIOff')
        f('bounce_off_velocity')
        for k in ('behavior', 'headBehavior', 'eyeBehavior', 'torsoBehavior'):
            i(k + '.present')
            if F[k + '.present']:
                F[k + '.class'] = skip_object(buf, p, end)[0]
                p = skip_object(buf, p, end)[3]
        for k in ('behaviorCode', 'headBehaviorCode', 'eyeBehaviorCode',
                  'torsoBehaviorCode'):
            i(k)
        i('haveThrowObject')
        s('animset')
        u('actor_flags1')
        u('actor_flags2')
        u('actor_flags3')
        u('actor_flags4')
        u('notify_flags1')
        u('state_flags')
        F['_flags_reached'] = True
    except (StopWalk, EventUnmodelled) as exc:
        F['_partial'] = True
        F['_partial_why'] = str(exc)
        F['_walk_end'] = p
        ent.fields = F
        return F
    F['_walk_end'] = p
    F['_complete'] = (p == end)
    F['_residual'] = end - p
    ent.fields = F
    return F


class StreamAbort(Exception):
    pass


class StopWalk(Exception):
    pass


def skip_object(buf, p, end):
    """Archiver::ReadObject record header -- return (classname, body, size,
    past_end) without modelling the body (archive.cpp:835-916)."""
    typ = i32(buf, p)
    size = u32(buf, p + 4)
    q = p + 8
    name, q = fstr(buf, q)
    if typ == S.ENT:
        q += 8                      # entnum + spawnflags
    q += 4                          # index
    body = q
    return (name, body, size, body + size)


def read_event_bytes(buf, p, end):
    """Consume one Event::Archive body with a throwaway Reader; return the new
    offset.  Used for ArchiveEventPointer's valid arm.  Raises StreamError when
    the body does not fit sav_stream's Event model."""
    r = S.Reader(buf, start=p, end=end, label='event')
    S.read_event(r)
    return r.pos


class EventUnmodelled(Exception):
    pass


def entities(buf):
    r = S.Reader(buf, label=os.path.basename(SAV))
    r.unsigned(); r.unsigned(); r.string(); r.integer()
    assert (r.integer(), r.integer()) == (4, 8)
    for _ in range(r.integer()):
        S.read_event(r); r.float32(); r.integer()
    w = S.Walker({})
    for _ in range(10):
        w.read_object(r, 0)
    n_edicts = r.integer()
    n_saved = r.integer()
    out = []
    for _ in range(n_saved + 1):
        typ = r.integer()
        size = r.unsigned()
        classname = r.string()
        entnum = spawnflags = None
        if typ == S.ENT:
            entnum = r.integer()
            spawnflags = r.integer()
        index = r.integer()
        body = r.pos
        r.skip_to(body + size)
        out.append((entnum, spawnflags, classname, body, size))
    return out, n_edicts, n_saved


def main():
    buf = open(SAV, 'rb').read()
    ents, n_edicts, n_saved = entities(buf)
    print('# %s  n_edicts=%d n_saved=%d ents=%d'
          % (os.path.basename(SAV), n_edicts, n_saved, len(ents)))
    rows = []
    for (entnum, spawnflags, classname, body, size) in ents:
        e = Ent()
        e.entnum, e.classname, e.body, e.size = entnum, classname, body, size
        e.fields, e.error, e.npc = None, None, False
        try:
            decode_entity(buf, e)
        except StreamAbort as exc:
            e.error = str(exc)
        except (struct.error, IndexError) as exc:
            e.error = '%s: %s' % (type(exc).__name__, exc)
        rows.append(e)

    npc_rows = [r for r in rows if r.classname in ('Actor',)]
    if "all" in sys.argv[1:]:
        npc_rows = rows
    print('# %d Actor entities of %d' % (
        len([r for r in rows if r.classname == 'Actor']), len(rows)))

    hdr = ('ent', 'eType', 'solid', 'contents', 'clipmask', 'svflags', 'eFlags',
           'parent', 'bindparent', 'ownerNum', 'clNum', 'gEnt', 'hp', 'maxhp',
           'flags', 'tkdmg', 'movetype', 'modelidx', 'anim', 'torso_anim',
           'state', 'model')
    print('# ' + ' '.join('%-9s' % h for h in hdr))
    for e in npc_rows:
        F = e.fields or {}
        if F.get('_isclient'):
            print('  %-9s <client>' % e.entnum)
            continue
        if e.error and not F:
            print('  %-9s ERROR %s' % (e.entnum, e.error))
            continue

        def g(k, d=-99999999):
            v = F.get(k)
            return v if v is not None else d
        print('  %-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s '
              '%-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s'
              % (e.entnum,
                 ET_NAMES.get(g('s.eType'), g('s.eType')),
                 SOLID_NAMES.get(g('s.solid'), g('s.solid')),
                 g('edict.contents'), g('edict.clipmask'), g('svflags'),
                 '0x%x' % g('s.eFlags') if g('s.eFlags') != -99999999 else '-',
                 g('s.parent'), g('s.bindparent'), g('edict.ownerNum'),
                 g('s.clientNum'), g('s.groundEntityNum'),
                 round(g('health'), 1), round(g('max_health'), 1),
                 '0x%x' % g('flags') if g('flags') != -99999999 else '-',
                 DAMAGE_NAMES.get(g('takedamage'), g('takedamage')),
                 MOVE_NAMES.get(g('movetype'), g('movetype')),
                 g('s.modelindex'), '0x%x' % g('s.anim') if g('s.anim') != -99999999 else '-',
                 '0x%x' % g('s.torso_anim') if g('s.torso_anim') != -99999999 else '-',
                 F.get('_state', '-'), os.path.basename(g('model', '-'))[:20]))

    # ---- the flag words -----------------------------------------------------
    print('\n=== ACTOR FLAG WORDS (actor_flags1..4 / notify_flags1 / state_flags) ===')
    for e in npc_rows:
        F = e.fields or {}
        if F.get('_isclient'):
            continue
        w = []
        for k in ('actor_flags1', 'actor_flags2', 'actor_flags3', 'actor_flags4',
                  'notify_flags1', 'state_flags'):
            if k in F:
                w.append('0x%08x' % F[k])
            else:
                w.append('--------')
        print('  ent=%-5s %s %s %s %s  notify=%s state=%s'
              % (e.entnum, w[0], w[1], w[2], w[3], w[4], w[5]))

    # ---- per-entity complete dump (only actors) -----------------------------
    print('\n=== FULL FIELD DUMP (Actor entities) ===')
    keys = None
    for e in npc_rows:
        F = e.fields or {}
        if F.get('_isclient') or not F:
            continue
        if keys is None:
            keys = [k for k in F if not k.startswith('s.bone_')]
        print('--- ent %d (%s) body=0x%x size=%d complete=%s residual=%s %s'
              % (e.entnum, e.classname, e.body, e.size,
                 F.get('_complete'), F.get('_residual'), e.error or ''))
        for k in keys:
            v = F.get(k, '<absent>')
            if isinstance(v, float):
                v = round(v, 4)
            elif isinstance(v, tuple):
                v = tuple(round(x, 3) for x in v)
            elif isinstance(v, bytes):
                v = '<%d bytes>' % len(v)
            elif isinstance(v, list) and len(v) > 6:
                v = '<%d items>' % len(v)
            print('    %-34s %s' % (k, v))
    return 0


if __name__ == '__main__':
    sys.exit(main())
