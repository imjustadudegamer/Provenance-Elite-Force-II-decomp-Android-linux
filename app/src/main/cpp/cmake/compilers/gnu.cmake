# GNU style (GCC/Clang) compiler specific settings

if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_C_COMPILER_ID MATCHES "^(Apple)?Clang$")
    return()
endif()

enable_language(ASM)

set(ASM_SOURCES
    ${SOURCE_DIR}/asm/ftola.c
    ${SOURCE_DIR}/asm/snapvector.c
)

add_compile_options(-Wall -Wshadow
    -Wformat=2  -Wformat-security
    -Wstrict-aliasing=2 -Wmissing-format-attribute
    -Wdisabled-optimization)

# C-only warning flags: GCC/Clang reject these for C++ (and print a
# "not valid for C++" warning for every C++ translation unit), so scope
# them to the C compile language only.
add_compile_options(
    $<$<COMPILE_LANGUAGE:C>:-Wimplicit>
    $<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes>
    $<$<COMPILE_LANGUAGE:C>:-Werror-implicit-function-declaration>)

add_compile_options(-Wno-format-zero-length -Wno-format-nonliteral)

# -Wstringop-overflow false-positive on botlib's GetClearedMemory (Q3A code;
# GCC models the calloc'd region as size 0 through one inlined path).
# uivk: GCC-ONLY -- clang has no -Wstringop-overflow at all, and passing an
# unknown -Wno-* to it prints -Wunknown-warning-option in every TU.
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-stringop-overflow)
    # The same false positive fires in GCC 13's LTO stage: the fortified memset is
    # diagnosed from the merged GIMPLE at link time (observed on provenanceef2/provenanceef2ded
    # linking botlib's be_ai_chat.c), so suppress it for links as well.
    add_link_options(-Wno-stringop-overflow)
endif()
# ...and its "maybe" variant, which is what the fortified memset in
# botlib's GetClearedMemory actually trips.  GCC 13 folded that variant into
# -Wstringop-overflow= and DELETED the separate option, so passing it there makes
# cc1 print an "unrecognized command-line option" note for every translation unit
# that has any other diagnostic.
if(CMAKE_C_COMPILER_VERSION VERSION_LESS 13)
    add_compile_options(-Wno-maybe-stringop-overflow)
endif()

# There are lots of instances of union based aliasing in the code
# that rely on the compiler not optimising them away, so disable it
add_compile_options(-fno-strict-aliasing)

# This is necessary to hide all symbols unless explicitly exported
# via the Q_EXPORT macro
#add_compile_options(-fvisibility=hidden)

add_compile_options(-Wno-comment)
# Treat no return type as error
add_compile_options(-Werror=return-type)

#
# ODR VIOLATIONS ARE FATAL AT THE LTO LINK.
#
# This build is `-flto -fno-fat-lto-objects`, so the linker merges objects that were
# compiled from different definitions of the same class.  When that happens GCC emits
# `-Wodr` / `-Wlto-type-mismatch` and then PICKS ONE LAYOUT ANYWAY -- the binary is not
# merely noisy, it is wrong, and the corruption is silent at runtime.
#
# It is reachable in exactly one way, and it is a workflow hazard, not a build-system bug:
# ninja stats an edge's inputs when it SCHEDULES the edge, so a source tree mutated WHILE
# ninja is running (a git merge/checkout/stash, an editor write, a second ninja in the same
# build dir) leaves objects that read the OLD headers but whose .o is written AFTER the
# change.  Their mtime is newer than the header they disagree with, so ninja considers them
# up to date forever, and no number of later incremental builds repairs it.  Reproduce it
# in ~20 seconds with `misc/odr-race-repro.sh`; avoid it with `misc/ninja-guard.sh`.
#
# Ninja's ordinary header tracking is CORRECT and was measured twice: a clean build at
# `26d25f1` followed by `git merge 9656a91` -- the S12 Animate re-parenting, which moves
# Animate, Trigger, Sentient, Projectile and Gib -- rebuilt 225 edges with 0 warnings and
# 0 ODR.  Do not "fix" the depfiles.  See docs/KNOWN_GROUND.md, "BUILD INTEGRITY".
#
# Promoting these two to errors is the part a wrapper script cannot do: it protects
# somebody who ran bare `ninja`, and it makes the poisoned binary impossible to produce
# rather than merely reported.  Verified: with -Werror=odr the poisoned link FAILS, `prog`
# is never written, and the failure is sticky until `ninja -t clean`.  A clean build of
# this tree emits zero of either, so this costs nothing when the build is honest.
#
# IF YOU HIT THIS: your build directory is poisoned.  `ninja -C build -t clean && ninja -C build`.
#
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND IPO_SUPPORTED)
    add_link_options(-Werror=odr -Werror=lto-type-mismatch)
endif()

set(ASM_SOURCES)

execute_process(COMMAND ${CMAKE_C_COMPILER} -v
                OUTPUT_VARIABLE COMPILER_OUTPUT
                ERROR_VARIABLE COMPILER_OUTPUT
                OUTPUT_STRIP_TRAILING_WHITESPACE)

list(APPEND CLIENT_LIBRARIES m)
list(APPEND SERVER_LIBRARIES m)

if(COMPILER_OUTPUT MATCHES "musl|freebsd|openbsd|netbsd")
    list(APPEND CLIENT_LIBRARIES execinfo)
    list(APPEND SERVER_LIBRARIES execinfo)
else()
    if(ANDROID)
        # uivk-android: bionic has clock_gettime in libc and ships no librt, so
        # the GLIBC-2.34-compat link below would fail to find -lrt.
    else()
        # For when using GLIBC versions older than 2.34
        list(APPEND CLIENT_LIBRARIES rt)
        list(APPEND SERVER_LIBRARIES rt)
    endif()
endif()