#!/usr/bin/env python3
"""Verify the ported Player::Archive layout against REAL 2003 retail savegames.

Method: for every Player frame in every save, try every candidate start offset
inside the frame body and forward-parse the field list below.  A candidate is
accepted only if it consumes bytes and lands EXACTLY on the frame end.  If
exactly one offset solves, the layout is proven for that frame -- a wrong field
order would either overrun, underrun, or (much more rarely) solve at a different
offset, and a wrong WIDTH anywhere shifts the landing point.

Corpus: <home>/Downloads/startrekeliteforce2_savegames_all.zip
        116 .sav files, all dated 2003-06-30.
NEVER verify against base/save/omconfig*/ -- those were written by THIS PORT and
merely reproduce our own bugs.
"""
import io
import os
import struct
import sys
import zipfile

ZIP = os.path.expanduser('~/Downloads/startrekeliteforce2_savegames_all.zip')
ARC_OBJECT, ARC_ENTITY = 1, 2
NULLPTR = -654321


class Parse(Exception):
    pass


class R:
    __slots__ = ('d', 'o', 'end', 'log')

    def __init__(self, d, o, end, log=False):
        self.d, self.o, self.end = d, o, end
        self.log = [] if log else None

    def _need(self, n):
        if self.o + n > self.end:
            raise Parse('overrun')

    def i32(self, n=''):
        self._need(4)
        v = struct.unpack_from('<i', self.d, self.o)[0]
        if self.log is not None:
            self.log.append((self.o, 4, n, v))
        self.o += 4
        return v

    def u32(self, n=''):
        self._need(4)
        v = struct.unpack_from('<I', self.d, self.o)[0]
        if self.log is not None:
            self.log.append((self.o, 4, n, v))
        self.o += 4
        return v

    def f32(self, n=''):
        self._need(4)
        v = struct.unpack_from('<f', self.d, self.o)[0]
        if self.log is not None:
            self.log.append((self.o, 4, n, round(v, 4)))
        self.o += 4
        return v

    def b(self, n=''):
        self._need(1)
        v = self.d[self.o]
        if self.log is not None:
            self.log.append((self.o, 1, n, v))
        self.o += 1
        return v

    def vec(self, n=''):
        self._need(12)
        v = struct.unpack_from('<3f', self.d, self.o)
        if self.log is not None:
            self.log.append((self.o, 12, n, tuple(round(x, 3) for x in v)))
        self.o += 12
        return v

    def raw(self, k, n=''):
        self._need(k)
        v = self.d[self.o:self.o + k]
        if self.log is not None:
            self.log.append((self.o, k, n, v.hex()))
        self.o += k
        return v

    def st(self, n=''):
        o0 = self.o
        self._need(4)
        L = struct.unpack_from('<I', self.d, self.o)[0]
        # a length that cannot fit, or absurd, kills this candidate fast
        if L > 1024 or self.o + 4 + L > self.end:
            raise Parse('bad string length')
        self.o += 4
        v = self.d[self.o:self.o + L]
        # archived strings are plain ASCII asset/state names
        if any(c < 9 or c > 126 for c in v):
            raise Parse('non-ascii string')
        self.o += L
        if self.log is not None:
            self.log.append((o0, 4 + L, n, repr(v.decode('latin1'))))
        return v.decode('latin1')


def parse_player(d, start, end, log=False):
    """Mirror of the ported Player::Archive (player.h), retail order."""
    r = R(d, start, end, log)

    r.st('state_Legs'); r.st('state_Torso')
    r.st('last_torso_anim_name'); r.st('last_leg_anim_name')
    r.i32('movecontrol'); r.i32('last_camera_type')
    r.i32('newActiveWeapon.weapon'); r.i32('newActiveWeapon.hand')
    r.i32('head_target'); r.f32('look_at_time')
    r.i32('targetEnemy'); r.b('targetEnemyLocked')
    r.i32('shield_active')
    r.i32('dual_wield_active')
    n = r.i32('dual_wield count')
    if not (0 <= n <= 64):
        raise Parse('dual_wield count')
    for i in range(n):
        r.st('dw[%d].name' % i); r.i32('dw[%d].hand' % i)
    for i in range(3):
        r.i32('holsteredWeapons[%d]' % i)
    for i in range(3):
        r.st('partAnim[%d]' % i)
    r.b('animdone_Legs'); r.b('animdone_Torso')
    r.vec('oldvelocity'); r.vec('old_v_angle'); r.vec('oldorigin')
    r.f32('animspeed'); r.f32('airspeed')
    r.st('currentCallVolume')
    r.raw(16, 'blend')
    r.f32('fov'); r.f32('_userFovChoice')
    r.i32('vehicle'); r.vec('v_angle')
    r.i32('buttons'); r.i32('new_buttons'); r.f32('respawn_time')
    r.i32('last_attack_button')
    r.f32('damage_blood'); r.f32('damage_alpha')
    r.vec('damage_blend'); r.vec('damage_from'); r.vec('damage_angles')
    r.f32('damage_count')
    r.f32('bonus_alpha')
    r.f32('next_drown_time'); r.f32('next_painsound_time'); r.f32('air_finished')
    r.i32('old_waterlevel'); r.f32('drown_damage')
    r.f32('_flashAlpha'); r.vec('_flashBlend')
    r.f32('_flashMaxTime'); r.f32('_flashMinTime')
    r.st('waitForState')
    r.i32('camera'); r.i32('actor_camera'); r.i32('cool_camera')
    r.i32('entity_to_watch')
    r.f32('maximumAngleToWatchedEntity'); r.b('watchEntityForEntireDuration')
    r.i32('playerCameraMode')
    r.f32('action_level')
    r.i32('music_current_mood'); r.i32('music_fallback_mood')
    r.f32('music_current_volume'); r.f32('music_saved_volume')
    r.f32('music_volume_fade_time')
    r.i32('reverb_type'); r.f32('reverb_level')
    r.i32('music_forced'); r.b('_allowMusicDucking'); r.b('_allowActionMusic')
    r.i32('gibbed'); r.f32('pain')
    r.i32('pain_dir'); r.i32('pain_type'); r.b('take_pain')
    r.f32('accumulated_pain'); r.f32('nextpaintime')
    r.b('knockdown'); r.b('canfall'); r.b('falling')
    r.i32('feetfalling'); r.vec('falldir')
    r.b('hardimpact')
    r.raw(60, 'last_ucmd')
    r.vec('base_rightfoot_pos'); r.vec('base_leftfoot_pos')
    r.vec('rightfoot_pos'); r.vec('leftfoot_pos')
    r.b('_onLadder'); r.vec('_ladderNormal')
    r.f32('_ladderTop'); r.f32('_nextLadderTime')
    r.i32('pm_lastruntime'); r.f32('animheight')
    r.vec('yaw_forward'); r.vec('yaw_left')
    r.i32('atobject'); r.f32('atobject_dist'); r.vec('atobject_dir')
    r.i32('toucheduseanim'); r.i32('useanim_numloops')
    r.i32('useitem_in_use'); r.i32('cool_item')
    r.st('cool_dialog'); r.st('cool_anim')
    r.i32('moveresult')
    r.i32('do_rise'); r.i32('weapons_holstered_by_code')
    r.i32('projdetonate')
    r.i32('yawing'); r.i32('yawing_left'); r.i32('yawing_right')
    r.i32('adjust_torso')
    r.vec('torsoAngles'); r.vec('headAngles')
    r.vec('headAimAngles'); r.vec('torsoAimAngles')
    r.f32('damage_multiplier')
    r.i32('fakePlayer_active'); r.i32('fakePlayer')
    r.b('dont_turn_legs')
    r.f32('specialMoveCharge'); r.f32('specialMoveEndTime')
    r.f32('specialMoveChargeTime')
    r.i32('points'); r.f32('playerKnockback'); r.f32('knockbackMultiplier')
    r.b('changedStanceTorso'); r.b('changedStanceLegs')
    r.i32('stanceNumber'); r.b('incomingMeleeAttack')
    r.f32('bendTorsoMult'); r.st('lastActionType')
    r.i32('meleeAttackFlags'); r.b('changingChar')
    for nm in ('meleeAttackerList', 'finishableList', 'finishingMoveList'):
        c = r.i32('%s count' % nm)
        if not (0 <= c <= 256):
            raise Parse('%s count' % nm)
        for i in range(c):
            r.i32('%s[%d]' % (nm, i))
    r.i32('_powerup'); r.i32('_rune'); r.i32('_holdableItem')
    r.f32('_nextEnergyTransferTime')
    r.b('_canTransferEnergy'); r.b('_doDamageScreenFlash'); r.b('_isThirdPerson')
    r.i32('_finishActor'); r.st('_finishState'); r.b('_doingFinishingMove')
    r.b('_autoSwitchWeapons')
    r.b('_usingEntity'); r.st('_attackType'); r.i32('_gameplayAnimIdx')
    r.b('_disableUseWeapon')
    r.i32('_targetSelectedHighlight'); r.i32('_targetLockedHighlight')
    r.b('_infoHudOn'); r.i32('_nextRegenTime'); r.f32('_useEntityStartTimer')
    r.st('objectiveName')
    r.u32('_objectiveStates'); r.u32('_informationStates')
    r.i32('_targetedEntity')
    r.i32('_dialogEntnum'); r.st('dialogSound'); r.st('dialogTextSound')
    r.f32('_crossHairXOffset'); r.f32('_crossHairYOffset')
    r.f32('_lastDamagedTimeFront'); r.f32('_lastDamagedTimeBack')
    r.f32('_lastDamagedTimeLeft'); r.f32('_lastDamagedTimeRight')
    r.i32('_totalGameFrames')
    r.f32('_nextPainShaderTime'); r.i32('_lastPainShaderMod')
    r.i32('_itemIcon'); r.st('_itemText'); r.st('_voteText')
    r.b('_validPlayerModel'); r.i32('_secretsFound')
    c = r.i32('_hudList count')
    if not (0 <= c <= 64):
        raise Parse('_hudList count')
    for i in range(c):
        r.st('_hudList[%d]' % i)
    r.i32('_skillLevel'); r.i32('_forcedMoveType')
    r.i32('_branchDialogActor')
    r.b('_cameraCutThisFrame')
    return r


def frames(path):
    """Yield (classname, body_start, body_end) for every ENTITY frame in a save.

    Walks the container exactly as tools/ef2_decode_sav.py does -- a heuristic
    byte-scan locks onto targetname strings and misses the real frames entirely.
    """
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__))))
    import ef2_decode_sav as E

    r = E.Reader(path)
    if r.raw(4) != b'EF2\x00':
        return
    r.i32(); r.string(); r.i32(); r.i32(); r.i32()
    for _ in range(r.i32()):                       # L_ArchiveEvents
        r.string(); r.raw(4); r.i32(); r.i16(); r.i16(); r.i32(); r.f32(); r.i32()
        [E.event_var(r) for _ in range(r.i32())]
        [E.event_argdef(r) for _ in range(r.i32())]
        if r.i32() == 1:
            E.read_object(r)
        r.f32(); r.i32()
    for _ in range(10):                            # ten fixed top-level objects
        E.read_object(r)
    r.i32(); r.i32()                               # num_entities, declared
    while True:
        o = E.read_object(r)
        if not o:
            break
        body = o['off'] + (r.o - o['off']) - o['size']
        yield o['cls'], body, body + o['size'], r.d


def solve(d, body, end):
    """Return every start offset from which the field list lands exactly on end."""
    hits = []
    for s in range(body, end):
        try:
            r = parse_player(d, s, end)
        except Parse:
            continue
        except Exception:
            continue
        if r.o == end:
            hits.append(s)
    return hits


def main(argv):
    dump = '--dump' in argv
    limit = None
    for a in argv:
        if a.startswith('--limit='):
            limit = int(a.split('=')[1])

    import tempfile

    z = zipfile.ZipFile(ZIP)
    savs = sorted(i for i in z.namelist() if i.lower().endswith('.sav'))
    if limit:
        savs = savs[:limit]

    tot = uniq = multi = zero = 0
    bad = []
    for name in savs:
        info = z.getinfo(name)
        if info.date_time[0] != 2003:
            print("SKIP (not 2003): %s" % name)
            continue
        tf = tempfile.NamedTemporaryFile(suffix='.sav', delete=False)
        tf.write(z.read(name))
        tf.close()
        try:
            found = list(frames(tf.name))
        except Exception as e:
            print("SKIP (walk failed: %s): %s" % (e, os.path.basename(name)))
            os.unlink(tf.name)
            continue
        os.unlink(tf.name)
        for cls, body, end, d in found:
            if cls != 'Player':
                continue
            tot += 1
            hits = solve(d, body, end)
            if len(hits) == 1:
                uniq += 1
                if dump:
                    r = parse_player(d, hits[0], end, log=True)
                    print("=== %s  frame %d..%d ===" % (os.path.basename(name), body, end))
                    for off, w, nm, val in r.log:
                        print("0x%06X %3d  %-30s %s" % (off, w, nm, val))
                    return 0
            elif len(hits) == 0:
                zero += 1
                bad.append((name, body, end, end - body, 'NO SOLUTION'))
            else:
                multi += 1
                bad.append((name, body, end, end - body, '%d solutions' % len(hits)))

    print("Player frames examined : %d" % tot)
    print("  unique solution      : %d" % uniq)
    print("  NO solution          : %d" % zero)
    print("  multiple solutions   : %d" % multi)
    for b in bad[:15]:
        print("   !! %s frame@%d size=%d -> %s" % (os.path.basename(b[0]), b[1], b[3], b[4]))
    return 0 if (tot and zero == 0 and multi == 0) else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
