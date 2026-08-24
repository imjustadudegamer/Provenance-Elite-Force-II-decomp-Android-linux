#!/usr/bin/env python3
"""R132-F4 corpus validator -- OFFLINE restore-path check over the 116-save retail corpus.

For every .sav it
  1. reads the sibling .ssv slot header (svs.time, map name, save label),
  2. byte-walks the .sav with the proven retail Archiver model (docs/tools/sav_stream.py)
     and records the true stage boundaries: entities end, Program end, alias block,
     TeammateRoster, GameplayManager, end-of-file,
  3. replays the PORT's alias/roster reader shape over the same bytes, both pre-fix and
     post-fix, and compares the cursor each one leaves at the gameplay-database entry,
  4. classifies the save:
       (a)  restores clean -- post-fix cursor lands exactly on the database, count 0
       (a*) restores clean cursor but a NON-ZERO gameplay-database count
       (b)  the GPM HANG class -- the pre-fix reader's misaligned cursor yields an
            absurd modified-object count
       (c)  another reader/writer asymmetry, or a structural failure -- named.

READ-ONLY: the corpus is referenced by absolute path and never copied.
"""
import os
import sys, os, struct, glob
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + '/docs/tools')
import sav_stream as S

FIELDS = S.load_class_table()
ROOT = (os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + '/docs/oracle/saves/'
        'retail_corpus_116')
MAX_TEAMMATES = 4


def ssv_info(ssv):
    b = open(ssv, 'rb').read()
    _id, svstime, _unk = struct.unpack_from('<iii', b, 0)

    def cstr(off):
        return b[off:b.find(b'\x00', off)].decode('latin1', 'replace')
    return svstime, cstr(76), cstr(12)


def walk(path):
    """Retail walk.  Returns a dict of stage boundaries or {'error': ...}."""
    buf = open(path, 'rb').read()
    r = S.Reader(buf, label=os.path.basename(path))
    try:
        r.unsigned(); r.unsigned()
        if r.string() != S.INFO.decode('latin1'):
            return {'error': 'bad archive header info'}
        r.integer()
        a, b2 = r.integer(), r.integer()
        if (a, b2) != (4, 8):
            return {'error': 'LevelArchiveValid magic %d/%d != 4/8' % (a, b2)}
        nevents = r.integer()
        for _ in range(nevents):
            S.read_event(r); r.float32(); r.integer()
        w = S.Walker(FIELDS)
        for _ in range(10):
            w.read_object(r, 0)
        n_edicts = r.integer(); n_saved = r.integer()
        for _ in range(n_saved + 1):
            w.read_object(r, 0)
        ents_end = r.pos
        w.read_object(r, 0)
        prog_end = r.pos
        al_start = r.pos
        al_b, al_n, al_bytes = S.read_aliases(r)
        al_end = r.pos
        ro_start = r.pos
        for _ in range(MAX_TEAMMATES):
            r.integer(); r.integer(); r.integer(); r.boolean1(); r.string()
        for _ in range(7):
            r.string()
        ro_end = r.pos
        gpm_start = r.pos
        nmod = r.integer()
        for _ in range(nmod):
            r.string(); np = r.integer()
            for _ in range(np):
                r.string(); r.string()
        gpm_end = r.pos
        return dict(filesize=len(buf), consumed=r.pos, n_saved=n_saved,
                    n_edicts=n_edicts, ents_end=ents_end, prog_end=prog_end,
                    al_start=al_start, al_buckets=al_b, al_nodes=al_n,
                    al_bytes=al_bytes, ro_bytes=ro_end - ro_start,
                    gpm_start=gpm_start, nmod=nmod, gpm_end=gpm_end)
    except Exception as exc:
        return {'error': '%s: %s' % (type(exc).__name__, exc)}


def port_cursor(path, al_start, fixed):
    """Replay the port's ArchiveAliases+TeammateRoster reader shape.

    fixed=False is the shape in the shipped R132 binary (no outer bucket-flag read).
    fixed=True  is this workstream's corrected shape, which is retail's.
    """
    buf = open(path, 'rb').read()
    pos = al_start

    def byte():
        nonlocal pos
        v = buf[pos]; pos += 1; return v

    def integer():
        nonlocal pos
        v = struct.unpack_from('<i', buf, pos)[0]; pos += 4; return v

    def string():
        nonlocal pos
        n = struct.unpack_from('<I', buf, pos)[0]; pos += 4; pos += n; return n
    try:
        another = byte()
        while another:
            string()
            another = byte()
            while another:
                string(); string(); string(); integer(); byte(); integer()
                another = byte()
                while another:
                    integer(); integer(); byte(); integer()
                    another = byte()
                if fixed:
                    another = byte()   # R131:F1:aliasdrift -- next alias flag
            if fixed:
                another = byte()       # R132-F4 -- next model-bucket flag
        # TeammateRoster::Archive, SDK verbatim
        for _ in range(MAX_TEAMMATES):
            integer(); integer(); integer(); byte(); string()
        for _ in range(7):
            string()
    except (IndexError, struct.error):
        # The pre-fix reader can run off the end of the buffer once its misaligned
        # cursor hands TeammateRoster a garbage string length.  That is itself the
        # signature being hunted; report the cursor as past-EOF.
        pass
    return pos


def main():
    # Optional alternate root (used to fold the operator's own saves into the same table).
    root = sys.argv[1] if len(sys.argv) > 1 else ROOT
    savs = sorted(glob.glob(root + '/**/*.sav', recursive=True))
    print("corpus .sav files: %d" % len(savs))
    rows = []
    cls = Counter()
    for sav in savs:
        base = os.path.basename(sav)
        player = os.path.basename(os.path.dirname(os.path.dirname(sav)))
        # the slot header sits beside the per-save directory, not inside it:
        #   .../save/<player>/SaveGameNNNN.ssv
        #   .../save/<player>/SaveGameNNNN/SaveGameNNNN.sav
        ssv = os.path.join(os.path.dirname(os.path.dirname(sav)),
                           os.path.basename(sav)[:-4] + '.ssv')
        if os.path.exists(ssv):
            svstime, mapname, comment = ssv_info(ssv)
        else:
            svstime, mapname, comment = -1, '<no .ssv>', ''
        res = walk(sav)
        if 'error' in res:
            c = '(c) structural failure'
            cls[c] += 1
            rows.append((mapname, svstime, '-', c, res['error'], base, player))
            continue
        exact = (port_cursor(sav, res['al_start'], True) == res['gpm_start'])
        tail_ok = (res['consumed'] == res['filesize'])
        pre_pos = port_cursor(sav, res['al_start'], False)
        buf = open(sav, 'rb').read()
        raw = buf[pre_pos:pre_pos + 4]
        pre_cnt = struct.unpack('<i', raw)[0] if len(raw) == 4 else 0
        if exact and tail_ok:
            if res['nmod'] == 0:
                c = '(a) restores clean'
                detail = ('gpm@%d count=0 eof-exact; alias %dB %dblk/%dnode'
                          % (res['gpm_start'], res['al_bytes'],
                             res['al_buckets'], res['al_nodes']))
            else:
                c = '(a*) clean cursor, NON-ZERO gpm count'
                detail = 'gpm@%d count=%d' % (res['gpm_start'], res['nmod'])
        elif exact:
            c = '(c) gpm cursor exact, tail not EOF'
            detail = 'consumed %d of %d' % (res['consumed'], res['filesize'])
        else:
            drift = res['gpm_start'] - port_cursor(sav, res['al_start'], True)
            if abs(pre_cnt) > 100000:
                c = '(b) GPM HANG class'
            else:
                c = '(c) other reader/writer asymmetry'
            detail = ('pre-fix cursor %d would read count %d; alias+roster short '
                      'by %d B' % (pre_pos, pre_cnt, drift))
        cls[c] += 1
        rows.append((mapname, svstime, res['n_saved'], c, detail, base, player))

    print("\n=== CLASSIFICATION TOTALS ===")
    for k, v in sorted(cls.items()):
        print("  %-40s %3d" % (k, v))
    print("\n=== PER-SAVE ===")
    print("%-26s %-9s %-5s %-34s %-22s %s"
          % ("map", "svs.time", "ents", "class", "detail", "save"))
    for r in sorted(rows):
        print("%-26s %-9s %-5s %-34s %-24s %s"
              % (r[0], r[1], r[2], r[3], r[4], r[5]))
    print("\n=== FAILED / CLASS-B SAVES (user test list) ===")
    for r in rows:
        if r[3].startswith('(b)') or r[3].startswith('(c)'):
            print("  %-26s %-9s %-34s %s [%s]"
                  % (r[0], r[1], r[3], r[4], r[6]))


if __name__ == '__main__':
    main()
