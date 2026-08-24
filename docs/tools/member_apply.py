#!/usr/bin/env python3
"""Apply the DEAD verdicts from docs/notes/ITEM_D_MEMBERS.md.  (v2)

v1 deleted wrong lines: it applied the note's absolute line numbers while
deleting, so every later line number in the same file was off by the number of
deletions already made there.  v2 collects every (file, line, text) edit
first, checks that the text on disk still matches what the note quoted, and
only then applies them -- DESCENDING by line number within each file, so no
edit invalidates a later one.

usage: member_apply.py [--dry] <class-section-heading> ...
"""
import os
import re
import subprocess
import sys

NOTE = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + '/docs/notes/ITEM_D_MEMBERS.md'
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + '/app/src/main/cpp'


def sh(*args):
    return subprocess.run(args, capture_output=True, text=True).stdout


def occurrences(name):
    out = sh('/bin/grep', '-rIn', '--include=*.[ch]', '--include=*.cpp', '--include=*.h',
             '-w', name, ROOT + '/code')
    return [l for l in out.split('\n') if l.strip()]


def parse_sections(headings):
    rows, cur = [], None
    for line in open(NOTE):
        if line.startswith('## '):
            cur = line[3:].strip()
        if cur is None or not line.startswith('| `'):
            continue
        if not any(h.lower() in cur.lower() for h in headings):
            continue
        cells = [c.strip() for c in line.strip().strip('|').split('|')]
        if len(cells) < 4 or not cells[3].startswith('DEAD'):
            continue
        rows.append({'member': cells[0].strip('`'), 'decl': cells[1],
                     'other': cells[2], 'verdict': cells[3]})
    return rows


def line_at(path, lineno):
    with open(path, errors='surrogateescape') as f:
        lines = f.read().split('\n')
    return lines[lineno - 1] if 1 <= lineno <= len(lines) else None


def statement_ok(text):
    s = text.strip()
    if not s or s.startswith('//') or s.startswith('*') or s.startswith('/*'):
        return False, 'comment/blank'
    if not s.endswith(';'):
        return False, 'does not end a statement'
    if re.search(r'\w\s*,\s*\w', s):
        return False, 'declarator list'
    return True, 'ok'


def main():
    args = sys.argv[1:]
    dry = '--dry' in args
    if dry:
        args.remove('--dry')
    rows = parse_sections(args)
    allnames = set(r['member'] for r in rows)
    print('%d DEAD rows parsed for %s' % (len(rows), args))

    edits, skipped = {}, []
    for r in rows:
        name = r['member']
        hits = occurrences(name)
        if len(hits) != 2:
            skipped.append((name, 'occurs %d times, not 2' % len(hits)))
            continue

        def resolve(cell):
            m = re.match(r'(\S+?):(\d+)', cell)
            return (ROOT + '/code/' + m.group(1), int(m.group(2))) if m else (None, None)

        dpath, dline = resolve(r['decl'])
        opath, oline = resolve(r['other'])
        if not dpath or not opath:
            skipped.append((name, 'cannot parse cite'))
            continue

        # both lines must still hold the member name -- this is the check v1
        # lacked, and it catches a shifted line number immediately
        for path, line, what in ((dpath, dline, 'decl'), (opath, oline, 'other')):
            text = line_at(path, line)
            if text is None or not re.search(r'\b%s\b' % re.escape(name), text):
                skipped.append((name, '%s line %d in %s does not contain the name'
                                % (what, line, path.split('/code/')[-1])))
                break
        else:
            dtxt = line_at(dpath, dline)
            others = [x for x in allnames if x != name
                      and re.search(r'\b%s\b' % re.escape(x), dtxt)]
            if others:
                skipped.append((name, 'decl line also names %s' % others))
                continue
            ok, why = statement_ok(dtxt)
            if not ok:
                skipped.append((name, 'decl %s' % why))
                continue
            ok2, why2 = statement_ok(line_at(opath, oline))
            if not ok2:
                skipped.append((name, 'other %s' % why2))
                continue
            edits.setdefault(dpath, []).append((dline, dtxt))
            edits.setdefault(opath, []).append((oline, line_at(opath, oline)))

    # apply descending per file
    applied = set()
    for path, items in edits.items():
        with open(path, errors='surrogateescape') as f:
            lines = f.read().split('\n')
        for line, expected in sorted(items, key=lambda t: -t[0]):
            if lines[line - 1] != expected:
                print('!! ABORT: %s:%d changed under us' % (path, line))
                sys.exit(1)
            del lines[line - 1]
            applied.add(path)
        if not dry:
            with open(path, 'w', errors='surrogateescape') as f:
                f.write('\n'.join(lines))

    per_member = len(applied)
    print('\nwould-delete %d member references across %d files' % (sum(len(v) for v in edits.values()), len(edits)))
    print('members: %d' % len(rows))
    print('\nskipped %d:' % len(skipped))
    for n, why in skipped:
        print('  %-34s %s' % (n, why))
    if not dry:
        print('\nDELETED MEMBERS:')
        for r in rows:
            if r['member'] not in [s[0] for s in skipped]:
                print('  ', r['member'])


if __name__ == '__main__':
    main()
