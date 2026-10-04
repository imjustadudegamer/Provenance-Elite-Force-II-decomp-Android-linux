#!/usr/bin/env python3
"""The picmip invariant, read off the SHIPPING tree.

`allowPicmip` is the flag that permits scaling; the number of levels to drop is
r_picmip->integer under that flag (retail Upload32 @0x00013000: the compressed
arm takes `v28 = r_picmip->integer` only when its allow flag is set, and the
uncompressed arm shifts by the count).  Passing the flag itself as the count
shifted every shader image by one level at r_picmip=0: the whole world corpus
uploaded at half extent while the UI, loaded with the flag off, stayed full.

  usage: picmipinvariant.py <code-dir> [--mutate N]
"""
import os
import re
import sys

TREES = {
    "vulkan": "renderervulkan/tr_image.c",
    "gles3": "renderergles3/tr_image.c",
}

MUTANT = ("int\t\t\tpicmip = allowPicmip ? r_picmip->integer : 0;",
          "int\t\t\tpicmip = allowPicmip;")

FAILS = []


def body(text, name):
    i = text.find(name + "(")
    if i < 0:
        return None
    j = text.find("{", i)
    depth = 0
    for k in range(j, len(text)):
        if text[k] == "{":
            depth += 1
        elif text[k] == "}":
            depth -= 1
            if depth == 0:
                return text[i:k + 1]
    return None


def check(cond, what):
    if cond:
        print("  ok    %s" % what)
    else:
        print("  FAIL  %s" % what)
        FAILS.append(what)


def main():
    code = sys.argv[1]
    mutate = 0
    if "--mutate" in sys.argv:
        mutate = int(sys.argv[sys.argv.index("--mutate") + 1])
    for tree, rel in sorted(TREES.items()):
        text = open(os.path.join(code, rel)).read()
        if mutate:
            old, new = MUTANT
            if text.count(old) >= 1:
                text = text.replace(old, new, 1)
            else:
                # the gles3 create is laid out with a tab less
                text = re.sub(r"int\s+picmip = allowPicmip \? r_picmip->integer : 0;",
                              "int picmip = allowPicmip;", text, count=1)
        create = body(text, "image_t* R_CreateImageOld") or ""
        m = re.search(r"picmip = allowPicmip \? r_picmip->integer : 0;", create)
        check(m is not None, "%s: the allow flag gates r_picmip's count" % tree)
        check("qboolean picmip" not in text,
              "%s: no dims/upload helper still takes the flag as a qboolean" % tree)
        # everything after the conversion is the create: the flag must not reach
        # a dims/upload call, and the count must
        tail = create[m.end():] if m else create
        check("allowPicmip," not in tail,
              "%s: no dims/upload call passes the allow flag" % tree)
        check(tail.count("picmip,") >= 3,
              "%s: the dims and upload calls take the count" % tree)
        if tree == "vulkan":
            check("if ( picmip > 0 ) {\n\t\tiStartImage = picmip;" in text
                  and "if ( picmip ) {\n\t\tsw >>= picmip;" in text,
                  "vulkan: both upload arms shift by the count")
    if FAILS:
        print("picmipinvariant: %d failures" % len(FAILS))
        return 1
    print("picmipinvariant: green")
    return 0


if __name__ == "__main__":
    sys.exit(main())
