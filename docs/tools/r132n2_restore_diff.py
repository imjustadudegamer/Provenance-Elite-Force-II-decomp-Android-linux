#!/usr/bin/env python3
"""R132-N2 -- the per-NPC, per-field RESTORE DIFF.

Side A: retail's own .sav bytes, decoded by r132n2_restore_decoder (this tree).
Side B: the port's live post-restore state, read out of a play_ef2 log
        ([R131:R1:ai] server sweep rows, [R131:K1:lerp] client entityState rows,
        [R128:anim:chan] client anim words).

Emits docs/agent_reports/ table rows: retail / port / verdict per field.

READ-ONLY.
"""
import glob
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SAV = sys.argv[1] if len(sys.argv) > 1 else (
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + '/personal/oracle/saves/retail_corpus_116/'
    'Elite Force II/base/save/Jens-Christian-Huus/SaveGame1057164666/'
    'SaveGame1057164666.sav')
LOGS = sorted(glob.glob(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + '/scratchpad/logs/'
                        'play_ef2-20260925-1*.log'))


def retail_table():
    out = subprocess.run(
        [sys.executable, os.path.join(HERE, 'r132n2_restore_decoder.py'), SAV],
        capture_output=True, text=True, check=True).stdout
    ents = {}
    cur = None
    for line in out.split('\n'):
        m = re.match(r'^--- ent (\d+) \((\S+)\) body=0x(\S+) size=(\d+)', line)
        if m:
            cur = int(m.group(1))
            ents[cur] = {'classname': m.group(2), 'body': m.group(3),
                         'size': int(m.group(4)), 'F': {}}
            continue
        if cur is None:
            continue
        m = re.match(r'^\s+(\S+)\s+(.*)$', line)
        if m and cur in ents:
            k, v = m.group(1), m.group(2).strip()
            ents[cur]['F'][k] = v
    return ents


def port_rows():
    """Pull the port's live per-entity rows out of the newest log."""
    best = {}
    for lp in LOGS:
        try:
            txt = open(lp, errors='replace').read()
        except OSError:
            continue
        klerp = {}
        for m in re.finditer(r'\[R131:K1:lerp\] ent=(\d+) etype=(\S+) interp=(\S+) '
                             r'pos\{t=(\S+) tm=(\S+) dur=(\S+) base=\(([^)]*)\) '
                             r'd=\(([^)]*)\)\} apos\{t=(\S+).*?\} '
                             r'angles=\(([^)]*)\) netang=\(([^)]*)\) '
                             r'org=\(([^)]*)\) netorg=\(([^)]*)\)', txt):
            e = int(m.group(1))
            klerp[e] = dict(etype=int(m.group(2)), interp=int(m.group(3)),
                            pos_t=int(float(m.group(4))), apos_t=int(float(m.group(9))),
                            angles=m.group(10), netang=m.group(11),
                            org=m.group(12), netorg=m.group(13))
        ai = {}
        for m in re.finditer(r'\[R131:R1:ai\] e=(\d+) t=(\d+) type=(\S+) mode=(\S+) '
                             r'think=(\S+) immob=(\S+) notgt=(\S+) started=(\S+) '
                             r'strat=(\S+) sm=(\S+) msm=(\S+) st=\'([^\']*)\' '
                             r'mst=\'([^\']*)\' enm=(\S+) list=(\S+) '
                             r'tend=(\S+) hp=(\S+) org=\(([^)]*)\) '
                             r'hull=\(([^)]*)\)-\(([^)]*)\) m=\'([^\']*)\'', txt):
            e = int(m.group(1))
            ai[e] = dict(actortype=int(m.group(3)), mode=int(m.group(4)),
                         think=int(m.group(5)), immob=int(m.group(6)),
                         notgt=int(m.group(7)), started=int(m.group(8)),
                         st=m.group(12), mst=m.group(13), enm=int(m.group(14)),
                         hp=m.group(17), org=m.group(18),
                         hull_min=m.group(19), hull_max=m.group(20),
                         model=m.group(21))
        chan = {}
        for m in re.finditer(r'\[R128:anim:chan\] ent=(\d+) '
                             r'anim=(\d+)\((\d+)\) torso=(\d+)\((\d+)\) '
                             r'blendL=(\d) blendT=(\d)', txt):
            e = int(m.group(1))
            chan[e] = dict(anim=int(m.group(2)), anim_idx=int(m.group(3)),
                           torso=int(m.group(4)), torso_idx=int(m.group(5)))
        # later logs win wholesale
        if klerp or ai:
            best = {'log': lp, 'klerp': klerp, 'ai': ai, 'chan': chan}
    return best


def canon(v):
    """Normalise a printed number / vector so retail's floats compare to the
    log's ints without format noise."""
    if v is None:
        return None
    s = str(v).strip().strip("'")
    if re.fullmatch(r'-?\d+(\.\d+)?', s):
        f = float(s)
        return round(f, 2)
    if re.fullmatch(r'-?\d+(\.\d+)?(,\s*-?\d+(\.\d+)?)+', s):
        return tuple(round(float(x), 2) for x in s.split(','))
    if re.fullmatch(r'-?\d+e-?\d+', s):
        return round(float(s), 4)
    return s


def main():
    rt = retail_table()
    pr = port_rows()
    print('# retail: %s' % os.path.basename(SAV))
    print('# port log: %s' % os.path.basename(pr.get('log', '<none>')))
    actors = sorted([e for e, d in rt.items() if d['classname'] == 'Actor'])

    rows = []
    missing = {'klerp': [], 'ai': [], 'chan': []}
    for e in actors:
        F = rt[e]['F']
        ai = pr.get('ai', {}).get(e)
        kl = pr.get('klerp', {}).get(e)
        ch = pr.get('chan', {}).get(e)
        for tag, obj in (('klerp', kl), ('ai', ai), ('chan', ch)):
            if obj is None:
                missing[tag].append(e)

        def cmp(name, retail, port, note=''):
            a, b = canon(retail), canon(port)
            same = (a == b)
            rows.append((e, name, retail, port, 'SAME' if same else 'DIFF', note))

        cmp('origin', F.get('origin'), (ai or {}).get('org'))
        cmp('angles', F.get('angles'), (kl or {}).get('angles'))
        cmp('netangles', F.get('netangles'), (kl or {}).get('netang'))
        if kl:
            cmp('s.pos.trType', '6', str(kl.get('pos_t')))
            cmp('s.eType', F.get('s.eType'), str(kl.get('etype')))
        if ai:
            cmp('health', F.get('health'), ai.get('hp'))
            cmp('edict.mins', F.get('edict.mins'), ai.get('hull_min'))
            cmp('edict.maxs', F.get('edict.maxs'), ai.get('hull_max'))
            cmp('model', os.path.basename(F.get('model', '').strip("'")),
                os.path.basename(ai.get('model', '').strip("'")))
        if ch:
            cmp('s.anim index', re.sub(r'\..*', '', str(int(float(F.get('s.anim', 0))) & 0x7FF)),
                str(ch.get('anim_idx')))
            cmp('s.torso_anim index', str(int(float(F.get('s.torso_anim', 0))) & 0x7FF),
                str(ch.get('torso_idx')))
    print('# actors with no row: klerp=%s ai=%s chan=%s'
          % (missing['klerp'], missing['ai'], missing['chan']))
    print('\n=== FIELD DIFF (retail .sav vs port live) ===')
    print('%-6s %-22s %-34s %-34s %-6s' % ('ent', 'field', 'retail', 'port', 'verdict'))
    cur = None
    ndiff = 0
    for (e, n, a, b, v, note) in rows:
        if e != cur:
            print('-' * 110)
            cur = e
        print('%-6s %-22s %-34s %-34s %-6s %s' % (e, n, str(a)[:34], str(b)[:34], v, note))
        if v == 'DIFF':
            ndiff += 1
    print('\n%d rows, %d DIFF' % (len(rows), ndiff))


if __name__ == '__main__':
    main()
