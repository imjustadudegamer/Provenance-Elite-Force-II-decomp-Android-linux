#!/usr/bin/env python3
# Census: every shipped shader stage whose DST factor is in the COLOR family
# (GL_SRC_COLOR / GL_ONE_MINUS_SRC_COLOR) -- the two ordinals the dst mapping
# changes.  Standalone; no import side effects.
import re, glob, os

def blocks(txt):
    out, cur = [], None
    for line in txt.split('\n'):
        s = line.strip()
        if s and not line[0].isspace() and not s.startswith('//'):
            if cur: out.append(cur)
            cur = (s, [])
        elif cur is not None:
            cur[1].append(line)
    if cur: out.append(cur)
    return out

def stages(body):
    if not isinstance(body, str):
        body = '\n'.join(body)
    res, stack = [], []
    for i, ch in enumerate(body):
        if ch == '{': stack.append(i)
        elif ch == '}':
            if stack: res.append(body[stack.pop()+1:i])
    return res

def main():
    rows = []
    for f in sorted(glob.glob('/tmp/r39pak/shaders/*.shader')):
        txt = open(f, errors='replace').read()
        for name, body in blocks(txt):
            if not name or name.startswith('//'): continue
            for st in stages(body):
                st = re.sub(r'//[^\n]*', '', st)
                bf = re.search(r'(?im)^\s*blendFunc\s+(\S+)\s+(\S+)', st)
                if not bf: continue
                if bf.group(2).upper() not in ('GL_SRC_COLOR', 'GL_ONE_MINUS_SRC_COLOR'):
                    continue
                g = re.search(r'(?im)^\s*rgbGen\s+(\S+(?:\s+\S+?)?)\s*$', st)
                rows.append((name, os.path.basename(f),
                             bf.group(1) + ' ' + bf.group(2),
                             g.group(1) if g else '-'))
    for r in rows:
        print('%-42s %-22s %-34s rgbGen=%s' % r)
    print('TOTAL stages whose dst is in the mapping class:', len(rows))

main()
