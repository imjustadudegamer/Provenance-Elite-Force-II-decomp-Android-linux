#!/usr/bin/env python3
"""Extract the world-set census decision region from the shipping vk_shade.c.

The census decides with three pure functions (is this image the bundle's own,
does the stage bind this unit at all, is a sample due).  They live between the
WORLD_SET_CENSUS markers in code/renderervulkan/vk_shade.c, so this script hands
the exact shipping text to the oracle -- no copy to drift.

With a mutation id it plants one of the faults this round fixed, in the source
text itself, so the oracle is shown to fail on the defect and not only pass on
the fix.

  usage: region_extract.py <vk_shade.c> [mutation-id]
"""
import sys

BEGIN = "/* WORLD_SET_CENSUS_BEGIN */"
END = "/* WORLD_SET_CENSUS_END */"

MUTANTS = {
    # the predicate bounded by numImageAnimations: image[0] drops out of the set
    1: ("\tn = bundle->numImageAnimations;\n"
        "\tif ( n < 1 ) {\n"
        "\t\tn = 1;\t\t\t/* image[0] is always a member of the set */\n"
        "\t}\n",
        "\tn = bundle->numImageAnimations;\n"),
    # the membership test can never match: a MISS when the image matches
    2: ("\t\tif ( cand == image ) {\n"
        "\t\t\treturn qtrue;\n"
        "\t\t}\n"
        "\t\tif ( cand->imgName[0] && image->imgName[0]\n"
        "\t\t\t && !strcmp( cand->imgName, image->imgName ) ) {\n"
        "\t\t\treturn qtrue;\n"
        "\t\t}\n",
        "\t\tcontinue;\n"),
    # every image is a member: a MISS suppressed when it does not match
    3: ("\treturn qfalse;\n}\n\n/* Whether the stage expects anything on a unit at all",
        "\treturn qtrue;\n}\n\n/* Whether the stage expects anything on a unit at all"),
    # the batch-counted cadence: unbounded in the world, no frame interval
    4: ("\tif ( lastSampleFrame == frame ) {\n"
        "\t\treturn qfalse;\t\t/* at most one agreeing row per frame */\n"
        "\t}\n"
        "\treturn ( frame % VK_WORLDSET_SAMPLE_FRAMES ) == 0 ? qtrue : qfalse;\n",
        "\treturn ( rows % 600 ) == 0 ? qtrue : qfalse;\n"),
    # an unbound unit is judged after all: the stale texImage[] row returns
    5: ("\treturn ( bundle && bundle->image[0] ) ? qtrue : qfalse;\n",
        "\treturn qtrue;\n"),
}


def main():
    if len(sys.argv) < 2:
        sys.stderr.write(__doc__)
        return 2
    path = sys.argv[1]
    mutant = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    text = open(path).read()
    i = text.find(BEGIN)
    j = text.find(END)
    if i < 0 or j < 0 or j < i:
        sys.stderr.write("no WORLD_SET_CENSUS markers in %s\n" % path)
        return 2
    body = text[i:j + len(END)]
    if mutant:
        old, new = MUTANTS[mutant]
        if body.count(old) != 1:
            sys.stderr.write("mutation %d does not match exactly once (%d)\n"
                             % (mutant, body.count(old)))
            return 2
        body = body.replace(old, new)
    sys.stdout.write(body + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
