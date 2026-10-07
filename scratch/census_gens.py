#!/usr/bin/env python3
# Proper brace-balanced shader parser: every stage's rgbGen and blendFunc.
# The earlier naive r'\{([^{}]*)\}' scan only matched the innermost group and
# silently dropped a stage's blendFunc whenever the stage held a nested brace.
import re, glob, os, sys

def blocks(txt):
    """Shader blocks start at a non-space column-0 token."""
    out, cur = [], None
    for line in txt.split('\n'):
        if line and not line[0].isspace() and not line.startswith('//'):
            if cur: out.append(cur)
            cur = [line.strip(), []]
        elif cur is not None:
            cur[1].append(line)
    if cur: out.append(cur)
    return out

def stages(body):
    """Every balanced brace group at every depth (a stage's own braces nest
    when the stage carries a second map, so top level alone under-reports)."""
    if not isinstance(body, str):
        body = '\n'.join(body)
    res, stack = [], []
    for i, ch in enumerate(body):
        if ch == '{':
            stack.append(i)
        elif ch == '}':
            if stack:
                s = stack.pop()
                res.append(body[s+1:i])
    return res

def main():
    ship = sys.argv[1] if len(sys.argv) > 1 else None
    rows = []
    for f in sorted(glob.glob('/tmp/r39pak/shaders/*.shader')):
        txt = open(f, errors='replace').read()
        for name, body in blocks(txt):
            if not name or name.startswith('//'): continue
            if ship and not any(s in name.lower() for s in ship.split(',')): continue
            for st in stages(body):
                # strip comments then read the directives
                st = re.sub(r'//[^\n]*', '', st)
                g  = re.search(r'(?im)^\s*rgbGen\s+(\S+(?:\s+\S+?)?)\s*$', st)
                bf = re.search(r'(?im)^\s*blendFunc\s+(\S+)\s+(\S+)', st)
                ag = re.search(r'(?im)^\s*alphaGen\s+(\S+)', st)
                if not (g or bf): continue
                rows.append((name, os.path.basename(f),
                             g.group(1) if g else '-', ag.group(1) if ag else '-',
                             bf.group(1)+' '+bf.group(2) if bf else '-'))
    for r in rows:
        print('%-46s %-20s rgbGen=%-22s alphaGen=%-12s blend=%s' % r)
    print('TOTAL stages named:', len(rows))

main()
