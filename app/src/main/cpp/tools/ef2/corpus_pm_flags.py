#!/usr/bin/env python3
"""Decode ps.pm_flags out of the 116 retail 2003 savegames.

The client image (playerState_t) is archived RAW inside every Player edict frame
(retail: ArchiveRaw(edict->client, sizeof(*edict->client)); port equivalent
G_ArchiveClientRetail, g_utils.cpp:1520).  Its head is, in retail order:

  0 commandTime  4 pm_type  8 bobCycle  12 pm_flags  16 pm_time  20 pm_runtime
  24 origin[3]  36 velocity[3]  48 gravity  52 speed  56 jumpvelocity
  60 crouchjumpvelocity  64 crouchjumpset  68 pm_stopspeed  72 pm_airaccelerate
  76 pm_wateraccelerate  80 pm_friction  84 pm_waterfriction  88 pm_accelerate
  92 pm_defaultviewheight

Those last seven are copied from the sv_* cvars every frame and their retail
defaults are known exactly (RE_PMOVE_EF2.md §2, cross-checked in gamex86.dll's
cvar registration block): 50, 2, 6, 6, 1, 10, 85.  That 28-byte run is a unique
anchor; pm_flags sits 56 bytes before it.  No layout inference required.
"""
import os
import struct
import sys
import zipfile

ZIP = os.path.expanduser('~/Downloads/startrekeliteforce2_savegames_all.zip')
ANCHOR = struct.pack('<7i', 50, 2, 6, 6, 1, 10, 85)

RETAIL = {
    0: 'DUCKED', 1: 'TIME_JUMP_START', 2: 'TIME_KNOCKBACK', 3: 'TIME_WATERJUMP',
    4: 'TIME_TELEPORT', 5: 'NO_PREDICTION', 6: 'FROZEN', 7: 'INTERMISSION',
    8: 'CAMERA_VIEW', 9: 'NO_MOVE', 10: 'HAVETARGET', 11: 'TIME_STUCKJUMP',
    12: 'LEVELEXIT', 13: 'NO_GRAVITY', 14: 'JUMP_HELD', 15: 'FLIGHT',
    16: 'ENEMY_TARGETED', 17: 'RADAR_MODE', 18: 'DISABLE_INVENTORY',
    19: '(unused19)', 20: '(unused20)', 21: '(unused21)',
    22: 'SCANNER', 23: 'ZOOM', 24: 'LEAN_LEFT', 25: 'LEAN_RIGHT',
    26: 'TIME_CROUCH_JUMP', 27: 'NIGHTVISION', 28: 'DAMAGE_FRONT',
    29: 'DAMAGE_BACK', 30: 'DAMAGE_LEFT', 31: 'DAMAGE_RIGHT',
}


def main():
    z = zipfile.ZipFile(ZIP)
    savs = sorted(i for i in z.namelist() if i.lower().endswith('.sav'))
    files = 0
    images = 0
    values = {}
    bitcount = {}
    pmtypes = {}
    pmtimes = {}
    vh = {}
    for name in savs:
        info = z.getinfo(name)
        if info.date_time[0] != 2003:
            print('SKIP (not 2003): %s' % name)
            continue
        d = z.read(name)
        files += 1
        pos = 0
        seen = []
        while True:
            i = d.find(ANCHOR, pos)
            if i < 0:
                break
            pos = i + 1
            base = i - 68
            if base < 0:
                continue
            ct, pt, bc, fl, tm, rt = struct.unpack_from('<6i', d, base)
            grav, spd = struct.unpack_from('<2i', d, base + 48)
            if not (0 < grav <= 2000) or not (0 < spd <= 1000):
                continue          # sanity on the two live cvar-driven fields
            seen.append((base, fl, pt, tm))
            images += 1
            values[fl] = values.get(fl, 0) + 1
            pmtypes[pt] = pmtypes.get(pt, 0) + 1
            pmtimes[tm] = pmtimes.get(tm, 0) + 1
            for b in range(32):
                if fl & (1 << b):
                    bitcount[b] = bitcount.get(b, 0) + 1
        if not seen:
            print('!! no client image found in %s' % name)
    print('files scanned          : %d' % files)
    print('client images found    : %d' % images)
    print()
    print('distinct pm_flags values:')
    for v, c in sorted(values.items(), key=lambda kv: -kv[1]):
        names = ' | '.join(RETAIL[b] for b in range(32) if v & (1 << b)) or '<none>'
        print('  0x%08X  x%-4d  %s' % (v & 0xFFFFFFFF, c, names))
    print()
    print('per-bit occupancy (retail names):')
    for b in range(32):
        c = bitcount.get(b, 0)
        if c:
            print('  bit %2d  %-18s %d' % (b, RETAIL[b], c))
    print()
    print('pm_type   : %s' % sorted(pmtypes.items()))
    print('pm_time   : %s' % sorted(pmtimes.items()))
    return 0


if __name__ == '__main__':
    sys.exit(main())
