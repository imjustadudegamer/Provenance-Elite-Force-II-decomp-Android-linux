#!/usr/bin/env python3
"""R132-G1 -- the PLAYER's Entity::flags word, decoded straight out of retail's .sav.

docs/tools/r132n2_restore_decoder.py only fully models Actor bodies, so the one
Player entity in the save (entnum 0, the save's only client) was never decoded and
the godmode adjudication had no player-side half.  This reuses that decoder's stream
helpers and walks exactly its own field order -- Entity::Archive head, the
G_ArchiveEdict gclient_t raw block (the player is the one edict with client != NULL),
the entityState_t, the edict frame, and the Entity::Archive tail -- stopping at
Entity::flags, which retail archives immediately after deadflag
(Mac Archive__6EntityFR8Archiver @+246: ArchiveInteger(a1+460)=health, +464=max_health,
+468=deadflag, +472=flags).

READ-ONLY.  Prints the player's flag word and the name of every set bit.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sav_stream as S
import r132n2_restore_decoder as D

SAV = sys.argv[1] if len(sys.argv) > 1 else (
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + '/personal/oracle/saves/retail_corpus_116/'
    'Elite Force II/base/save/Jens-Christian-Huus/SaveGame1057164666/'
    'SaveGame1057164666.sav')

FL_NAMES = {
    0: 'FL_FLY', 1: 'FL_SWIM', 2: 'FL_INWATER', 3: 'FL_GODMODE',
    4: 'FL_NOTARGET', 5: 'FL_PARTIALGROUND', 6: 'FL_TEAMSLAVE',
    7: 'FL_NO_KNOCKBACK', 8: 'FL_THINK', 9: 'FL_BLOOD', 10: 'FL_DIE_GIBS',
    11: 'FL_DIE_EXPLODE', 12: 'FL_ROTATEDBOUNDS', 13: 'FL_DONTSAVE',
    14: 'FL_IMMOBILE', 15: 'FL_PARTIAL_IMMOBILE', 16: 'FL_STUNNED',
    17: 'FL_POSTTHINK', 18: 'FL_TOUCH_TRIGGERS', 19: 'FL_AUTOAIM',
}


def main():
    buf = open(SAV, 'rb').read()
    ents, _, _ = D.entities(buf)
    hits = [e for e in ents if e[2] == 'Player']
    if not hits:
        print('no Player entity in %s' % os.path.basename(SAV))
        return 1
    entnum, _sp, _cn, body, size = hits[0]
    end = body + size
    p = body
    F = {}

    class Stop(Exception):
        pass

    def need(n):
        if p + n > end:
            raise Stop()

    def v3(k):
        nonlocal p
        need(12)
        F[k] = D.vec(buf, p)
        p += 12

    def v4(k):
        nonlocal p
        need(16)
        F[k] = D.vec4(buf, p)
        p += 16

    def i(k):
        nonlocal p
        need(4)
        F[k] = struct.unpack_from('<i', buf, p)[0]
        p += 4

    def u(k):
        nonlocal p
        need(4)
        F[k] = struct.unpack_from('<I', buf, p)[0]
        p += 4

    def f(k):
        nonlocal p
        need(4)
        F[k] = struct.unpack_from('<f', buf, p)[0]
        p += 4

    def raw(k, c):
        nonlocal p
        need(c)
        F[k] = buf[p:p + c]
        p += c

    def s(k):
        nonlocal p
        need(4)
        v, p2 = D.fstr(buf, p)
        F[k] = v
        p = p2

    try:
        # ------------------------------------------------ Entity::Archive head
        v3('_localOrigin')
        i('_lastTouchedList.count')
        n = F['_lastTouchedList.count']
        need(4 * n)
        p += 4 * n
        need(1)
        p += 1                       # _fulltrace
        i('_groupID')
        # the player is the one edict with client != NULL -> gclient_t raw block
        raw('client.raw', D.SZ_GCLIENT)
        i('s.instanceNumber')
        i('s.eType')
        i('s.eFlags')
        for _ in range(6):
            v3('s.vec')              # netorigin origin origin2 netangles angles viewangles
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
        for _ in range(D.NUM_BONE_CONTROLLERS):
            i('s.bone_tag')
            v3('s.bone_angles')
            v4('s.bone_quat')
        raw('s.morph_controllers', D.SZ_MORPH)
        raw('s.surfaces', D.SZ_SURFACES)
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
        raw('s.effectsAnims', D.SZ_EFFECTSANIMS)
        i('s.bindparent')
        v4('s.quat')
        raw('s.mat', D.SZ_MAT)
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
        # -------------------------------------------------- Entity::Archive tail
        s('_archetype')
        need(1)                      # _missionObjective -- ArchiveBool, 1 byte
        p += 1
        s('_targetPos')
        need(1)                      # _networkDetail   -- ArchiveBool, 1 byte
        p += 1
        i('spawnflags')
        s('model')
        v3('total_delta')
        v3('mins')
        v3('maxs')
        v3('absmin')
        v3('absmax')
        v3('centroid')
        v3('velocity')
        v3('avelocity')
        v3('origin')
        v3('angles')
        v3('size')
        i('movetype')
        i('mass')
        f('gravity')
        raw('orientation', D.SZ_MAT)
        v3('localangles')
        i('groundentity')
        raw('groundplane', D.SZ_CPLANE)
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
    except Stop:
        print('(stream end)')
    except Exception as exc:         # noqa: BLE001
        print('stopped: %s: %s' % (type(exc).__name__, exc))

    print('ent %d class Player  body=0x%x size=%d  consumed %d'
          % (entnum, body, size, p - body))
    for k in ('s.eType', 's.eFlags', 's.clientNum', 's.modelindex', 's.solid',
              'edict.entname', 'edict.contents', 'edict.clipmask', 'svflags',
              'spawnflags', 'model', 'health', 'max_health', 'deadflag',
              'flags', 'takedamage', 'damage_type'):
        v = F.get(k)
        if k == 'flags' and isinstance(v, int):
            v = '0x%08x' % (v & 0xffffffff)
        print('  %-18s %s' % (k, v))
    fl = F.get('flags')
    if isinstance(fl, int):
        print('')
        print('  PLAYER flags word = 0x%08x' % (fl & 0xffffffff))
        print('  bit 3 FL_GODMODE SET = %s' % bool(fl & 0x8))
        print('  bits set: %s' % [(b, FL_NAMES.get(b, '?'))
                                  for b in range(32) if fl & (1 << b)])
    return 0


if __name__ == '__main__':
    sys.exit(main())
