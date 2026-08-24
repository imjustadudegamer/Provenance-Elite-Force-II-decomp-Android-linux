#!/usr/bin/env python3
"""Delete only the preprocessor lines that are provably compiled out.

Retail EF2 has no curl / mumble / voip / vorbis / opus, verified against
docs/evidence/mac_symbols/macbin/EF2_mac_engine.pef: zero occurrences of mumble, voip,
opus, vorbis and openal; the lone `curl' hit is the literal "curly brace".
The port carries the ioquake3 `#ifdef'd implementations, compiled out today
but left as dead text.

SAFETY (fourth version of this tool; v1 destroyed qcommon.h by evaluating
`#ifndef _QCOMMON_H_' as false; v2/v3 unbalanced `#if/#else' pairs and ate the
live `#else' arm of `#ifdef DEDICATED'):

  * NO directive line is ever rewritten or removed -- the output is a pure
    line deletion, so `#if ... #endif' nesting survives exactly as it was.
  * A branch BODY is dropped only when every earlier branch of the same frame
    is PROVABLY dead and this branch is too.  Provably dead means:
        `#ifdef D' / `#if defined D' / `#if defined(D)'          (D in DEAD set)
        `A && B'  when any term is provably dead
        `A || B'  when every alternative is provably dead
  * `#ifndef D' is TRUE when D is dead, so never dropped.
  * An `#else' arm is never dropped -- the tool never assumes an unknown
    option is defined, so it cannot prove "none of the above".
  * Anything that does not mention a dead option -- include guards,
    `#ifdef __cplusplus', `#ifdef DEDICATED', `#ifdef _WIN32' -- is untouched.

usage: stripcpp.py <file> <DEADNAME>...
"""
import re
import sys

DIR = re.compile(r'^[ \t]*#[ \t]*(if|ifdef|ifndef|else|elif|endif)\b[ \t]*(.*?)[ \t]*$')


def split_top(expr, sep):
    out, depth, cur, i = [], 0, '', 0
    while i < len(expr):
        c = expr[i]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
        elif depth == 0 and expr.startswith(sep, i):
            out.append(cur)
            cur = ''
            i += len(sep)
            continue
        cur += c
        i += 1
    out.append(cur)
    return out


def atom_is_dead(a, dead):
    t = a.strip()
    if t.startswith('!'):
        return False                      # negation: not provably false
    t = re.sub(r'^defined\s*', '', t).strip()
    if t.startswith('(') and t.endswith(')'):
        t = t[1:-1].strip()
    return t in dead


def expr_is_dead(expr, dead):
    """True only when the expression provably evaluates to FALSE."""
    e = expr.strip()
    if not e or not any(re.search(r'\b%s\b' % d, e) for d in dead):
        return False
    for term in split_top(e, '&&'):
        if atom_is_dead(term, dead):
            return True
    alts = split_top(e, '||')
    if len(alts) > 1 and all(atom_is_dead(a, dead) for a in alts):
        return True
    return False


def process(path, dead):
    with open(path, 'r', errors='surrogateescape') as f:
        lines = f.read().split('\n')

    out, stack, dropped = [], [], 0

    for ln in lines:
        m = DIR.match(ln)
        if not m:
            if stack and not stack[-1]['emit']:
                dropped += 1
                continue
            out.append(ln)
            continue
        kw, arg = m.group(1), m.group(2)
        out.append(ln)                    # a directive line is never removed

        if kw in ('if', 'ifdef', 'ifndef'):
            isdead = expr_is_dead(arg, dead)
            stack.append({'emit': not isdead, 'prevdead': isdead})
        elif kw == 'elif':
            isdead = expr_is_dead(arg, dead) and stack[-1]['prevdead']
            stack[-1]['emit'] = not isdead
            stack[-1]['prevdead'] = stack[-1]['prevdead'] and isdead
        elif kw == 'else':
            stack[-1]['emit'] = True      # never provably dead
            stack[-1]['prevdead'] = False
        elif kw == 'endif':
            stack.pop()

    assert not stack, '%s: unbalanced preprocessor stack' % path
    with open(path, 'w', errors='surrogateescape') as f:
        f.write('\n'.join(out))
    return dropped


if __name__ == '__main__':
    p = sys.argv[1]
    n = process(p, set(sys.argv[2:]))
    if n:
        print('%-50s %d lines dropped' % (p, n))
