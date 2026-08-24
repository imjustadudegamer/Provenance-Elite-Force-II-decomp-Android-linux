#!/usr/bin/env python3
"""F5 -- decode the ARCHIVED WEAPON STATE out of a retail .sav (READ-ONLY).

Anchors on the per-firemode `projectileModel' ArchiveString inside retail's
Weapon::Archive (SDK weapon.h:683-905) and walks FORWARD, which avoids needing a
model of Entity::Archive/Animate::Archive/Item::Archive to find the block start.

READ-ONLY.  Usage:  python3 /tmp/wpn_state.py [sav] [entnum ...]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + '/docs/tools')
import sav_stream as S
from sav_stream import StreamError

DEFAULT = (os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + '/scratchpad/airun/base/save/adam/'
           'SaveGame1057164666/SaveGame1057164666.sav')
MAX_FIREMODES = 3

FIELD_ORDER_AFTER_PROJECTILE = [
    ('bulletdamage', 'f'), ('bulletcount', 'f'), ('bulletrange', 'f'),
    ('bulletknockback', 'f'), ('projectilespeed', 'f'),
    ('bulletspread', 'v'), ('endbulletspread', 'v'),
    ('firetype', 'i'), ('ammo_clip_size', 'i'), ('ammo_in_clip', 'i'),
    ('max_charge_time', 'f'), ('meansofdeath', 'i'), ('loopfire', 'i'),
    ('action_level_increment', 'i'), ('worldhitspawn', 's'),
    ('next_fire_time', 'f'), ('fire_timer', 'f'), ('accuracy100', 'r100'),
    ('burstmode', 'i'), ('_burstModeDelay', 'f'),
    ('shootingMoveSpeedModifier', 'f'), ('_viewShakeMagnitude', 'f'),
    ('_viewShakeDuration', 'f'), ('_viewMinShake', 'v'), ('_viewMaxShake', 'v'),
    ('_viewShakeOverride', 'c'), ('_noAmmoMode', 'c'), ('_noDelay', 'c'),
    ('_chargedModels', 'i'), ('_spreadAnims', 'i'), ('_spreadTime', 'f'),
    ('_meleeWidth', 'f'), ('_meleeHeight', 'f'), ('_meleeLength', 'f'),
    ('_fireOffset', 'v'), ('_regenAmount', 'i'), ('_regenTime', 'f'),
    ('_regenOnlyWhenIdle', 'c'), ('_nextRegenTime', 'f'),
]

FIELD_ORDER_MODE_HEAD = [
    ('ammo_type', 's'), ('ammorequired', 'i'), ('startammo', 'i'),
    ('_ammoBoost', 'i'), ('projectileModel', 's'),
]

EPILOGUE_AFTER_MODES = [
    ('_maxViewShakeChange', 'f'), ('startfiretime', 'f'),
    ('autoAimTargetSelectionAngle', 'i'), ('autoAimLockonAngle', 'i'),
    ('charge_fraction', 'f'), ('putaway', 'i'), ('firemodeindex', 'i'),
    ('hand', 'i'), ('reticuletime', 'f'), ('zoomfov', 'f'),
    ('_lastZoomFov', 'f'), ('startzoom', 'f'), ('endzoom', 'f'),
    ('zoomtime', 'f'), ('startzoomtime', 'f'), ('aimtype', 'i'),
    ('usesameclip', 'i'), ('chargetime', 'f'), ('targetidle', 'i'),
    ('donefiring', 'i'), ('zoomed', 'i'), ('targetingSkin', 'i'),
    ('shootingSkin', 'i'), ('_fullAmmoSkin', 'i'), ('_fullAmmoMode', 'i'),
    ('defaultMoveSpeedModifier', 'f'), ('viewShake', 'v'),
    ('_controllingProjectile', 'c'), ('_controllingProjectileHidden', 'c'),
    ('_controlEmitterName', 's'), ('_controlSoundName', 's'),
    ('_canInterruptFiringState', 'c'), ('_zoomStage', 'i'),
    ('_autoReload', 'c'), ('_allowAutoSwitch', 'c'), ('_nextSwitchTime', 'f'),
]


def read_field(buf, p, kind, d, name):
    if kind == 's':
        n = struct.unpack_from('<I', buf, p)[0]
        if n > 4096:
            raise StreamError('absurd string len %d for %s' % (n, name))
        d[name] = buf[p + 4:p + 4 + n].decode('latin1')
        return p + 4 + n
    if kind == 'v':
        d[name] = struct.unpack_from('<3f', buf, p)
        return p + 12
    if kind == 'r100':
        d[name] = struct.unpack_from('<25f', buf, p)[0]
        return p + 100
    if kind == 'f':
        d[name] = struct.unpack_from('<f', buf, p)[0]
        return p + 4
    if kind == 'c':
        d[name] = buf[p]
        return p + 1
    d[name] = struct.unpack_from('<i', buf, p)[0]
    return p + 4


def entities(buf):
    mod = {}
    src = open(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + '/docs/scratch_w2/'
               'ent_solid_scan.py').read().split('def decode_edict')[0]
    exec(compile(src.replace('#!/usr/bin/env python3', ''), 'x1', 'exec'), mod)
    return mod['entities'](buf, 'f5')


def main():
    sav = DEFAULT
    argv = sys.argv[1:]
    if argv and argv[0].endswith('.sav'):
        sav = argv[0]
        argv = argv[1:]
    want = set(int(x) for x in argv)
    buf = bytearray(open(sav, 'rb').read())
    ents, n_edicts, n_saved = entities(buf)
    print('save=%s  edicts=%d  saved=%d' % (os.path.basename(sav), n_edicts, n_saved))
    for entnum, classname, body, size in ents:
        if classname != 'Weapon':
            continue
        if want and entnum not in want:
            continue
        lo, hi = body, body + size
        anchors = []
        i = lo
        while i < hi - 4:
            if buf[i] == ord('p') and buf[i + 1] == ord('r'):
                n = struct.unpack_from('<I', buf, i - 4)[0]
                if 8 <= n <= 96 and i + n <= hi:
                    s = buf[i:i + n].decode('latin1')
                    if 'projectile' in s and (s.endswith('.tik') or s.endswith('.spr')):
                        anchors.append((i - 4, n, s))
            i += 1
        print('---- ent=%d body=0x%x size=%d  projectileModel anchors=%d'
              % (entnum, body, size, len(anchors)))
        if not anchors:
            print('     (no anchor)')
            continue
        a0, n0, s0 = anchors[0]
        boost = struct.unpack_from('<i', buf, a0 - 4)[0]
        startammo = struct.unpack_from('<i', buf, a0 - 8)[0]
        ammorequired = struct.unpack_from('<i', buf, a0 - 12)[0]
        nl = struct.unpack_from('<I', buf, a0 - 16)[0]
        at = buf[a0 - 12:a0 - 12 + nl].decode('latin1') if 0 < nl <= 64 else '?'
        print('     MODE1 ammo_type=%r ammorequired=%d startammo=%d _ammoBoost=%d'
              % (at, ammorequired, startammo, boost))
        # back to the start of mode 1's block
        p = a0 - 16
        d = {}
        try:
            for mi in range(MAX_FIREMODES):
                for name, kind in FIELD_ORDER_MODE_HEAD:
                    p = read_field(buf, p, kind, d, 'm%d.%s' % (mi + 1, name))
                for name, kind in FIELD_ORDER_AFTER_PROJECTILE:
                    p = read_field(buf, p, kind, d, 'm%d.%s' % (mi + 1, name))
            for name, kind in EPILOGUE_AFTER_MODES:
                p = read_field(buf, p, kind, d, name)
        except Exception as e:
            print('     FORWARD DECODE FAIL at 0x%x: %s' % (p, e))
            continue
        for mi in range(1, MAX_FIREMODES + 1):
            print('     MODE%d ammo_type=%r ammoreq=%d clip=%d/%d loopfire=%d '
                  'firetype=%d next_fire_time=%.3f noAmmo=%d proj=%r'
                  % (mi, d['m%d.ammo_type' % mi], d['m%d.ammorequired' % mi],
                     d['m%d.ammo_in_clip' % mi], d['m%d.ammo_clip_size' % mi],
                     d['m%d.loopfire' % mi], d['m%d.firetype' % mi],
                     d['m%d.next_fire_time' % mi], d['m%d._noAmmoMode' % mi],
                     d['m%d.projectileModel' % mi]))
        print('     EPILOGUE firemodeindex=%d hand=%d putaway=%d zoomStage=%d '
              '_autoReload=%d _allowAutoSwitch=%d donefiring=%d zoomed=%d '
              'usesameclip=%d aimtype=%d ctrlProj=%d/%d canInterrupt=%d'
              % (d['firemodeindex'], d['hand'], d['putaway'], d['_zoomStage'],
                 d['_autoReload'], d['_allowAutoSwitch'], d['donefiring'],
                 d['zoomed'], d['usesameclip'], d['aimtype'],
                 d['_controllingProjectile'], d['_controllingProjectileHidden'],
                 d['_canInterruptFiringState']))
        print('     end-of-Weapon::Archive at 0x%x (body end 0x%x, residual %d)'
              % (p, hi, hi - p))


main()
