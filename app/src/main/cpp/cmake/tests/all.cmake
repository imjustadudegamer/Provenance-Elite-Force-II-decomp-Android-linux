enable_testing()

# TIKIANIM-C: include(tests/lz77) is gone with code/corepp/lz77.cpp and
# code/corepp/tests/test_lz77.cpp -- a MOHAA codec with zero retail presence and no
# caller in the tree.  This all.cmake was itself not included by any build
# (build.ninja had no test_lz77 target), so the fragment is deleted rather than
# re-pointed.
