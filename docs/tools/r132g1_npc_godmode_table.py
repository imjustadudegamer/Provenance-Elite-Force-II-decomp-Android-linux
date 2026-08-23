#!/usr/bin/env python3
"""R132-G1 -- test the port's FL_GODMODE bit against every decoded NPC flag word.

Reads SaveGame1057164666.sav through docs/tools/r132n2_restore_decoder.py (READ-ONLY)
and prints, for all 30 restored Actor NPCs, the Entity::flags word retail archived,
which bits are set, whether the godmode bit (bit 3, FL_GODMODE) is set, and takedamage.
Companion to r132g1_player_flags.py, which does the same for the one Player entity.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import r132n2_restore_decoder as D

SAV = sys.argv[1] if len(sys.argv) > 1 else (
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + '/personal/oracle/saves/retail_corpus_116/'
    'Elite Force II/base/save/Jens-Christian-Huus/SaveGame1057164666/'
    'SaveGame1057164666.sav')

buf = open(SAV, 'rb').read()
ents, _, _ = D.entities(buf)

print('ent  Entity::flags  bits-set                            '
      'FL_GODMODE(bit3)  takedamage')
ngod = 0
n = 0
for (entnum, _sp, cn, body, size) in ents:
    if cn != 'Actor':
        continue
    e = D.Ent()
    e.entnum, e.classname, e.body, e.size = entnum, cn, body, size
    e.fields, e.error, e.npc = None, None, False
    try:
        D.decode_entity(buf, e)
    except Exception:                     # noqa: BLE001
        pass
    F = e.fields or {}
    fl = F.get('flags')
    td = F.get('takedamage')
    bits = [b for b in range(32) if fl and (fl >> b) & 1]
    g = bool(fl and (fl & 8))
    if g:
        ngod += 1
    n += 1
    print('%-4d 0x%08x   %-34s %-16s %s'
          % (entnum, fl & 0xffffffff, bits, 'SET' if g else 'clear', td))

print('')
print('NPCs with the godmode bit SET: %d of %d' % (ngod, n))
