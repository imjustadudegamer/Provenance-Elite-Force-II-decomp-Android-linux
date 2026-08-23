if(NOT BUILD_GAME_LIBRARIES AND NOT BUILD_GAME_QVMS)
    return()
endif()

include(utils/set_output_dirs)
include(shared_script)

file(GLOB_RECURSE BG_SOURCES
    ${SOURCE_DIR}/fgame/bg_misc.cpp
    ${SOURCE_DIR}/fgame/bg_pmove.cpp
)

# CONFIGURE_DEPENDS here for the same reason it is on GAME_SOURCES below: without it
# an added cgame source is silently absent from an EXISTING build dir, and ninja
# reports "no work to do" while the link leaves the new file's symbols undefined.
# That is exactly how cg_radar.cpp came through as `undefined symbol: CG_Target` --
# a fresh configure hid it, an incremental build did not.
file(GLOB_RECURSE CGAME_SOURCES CONFIGURE_DEPENDS
    ${SOURCE_DIR}/cgame/*.c ${SOURCE_DIR}/cgame/*.cpp
)

set(CGAME_BINARY_SOURCES)
set(CGAME_QVM_SOURCES)

# CONFIGURE_DEPENDS ensures newly added source files (e.g. EF2 fgame/cinematic.cpp)
# are picked up on the next build without a manual cmake re-run.
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
    ${SOURCE_DIR}/fgame/*.c ${SOURCE_DIR}/fgame/*.cpp
    ${SOURCE_DIR}/script/*.c ${SOURCE_DIR}/script/*.cpp
    # NOTE: the retail game.so DEFINES the Gameplay* classes itself (Mac binary:
    # GameplayManager::create @0x1f2380, GameplayDatabase::parseFile @0x1ecd60,
    # GameplayFormulaManager::getFormulaResult @0x1f0e00, ...).  The SDK ships
    # them in Shared/qcommon because BOTH binaries compile their own copy, so
    # they are linked into game.so from GAME_MODULE_SHARED_SOURCES below -- no
    # dlopen/-rdynamic resolution involved.
)

# NOTE: the Q3/MOHAA bot-AI exclusions that used to sit here are gone.  The
# ai_chat/ai_cmd/ai_dmnet/ai_dmq3/ai_main/ai_team/ai_vcmd/g_bot family IS retail
# EF2: the Mac GAME binary carries 196 Bot*/AINode_* + 8 AIEnter_* symbols plus the
# G_AddBot / G_AddRandomBot / G_BotConnect / G_CheckBotSpawn / G_CountBotPlayers /
# G_CheckBotSpawn / G_GetBotInfoByName / G_InitBots / G_LoadBots / G_LoadBotsFromFile /
# G_SpawnBots / SV_AddBot_f glue (personal/evidence/mac_symbols/game_dll_symbols.txt:1156-1263,
# 1326-1333, 4092-4104), the engine reaches it through the BOTAI_START_FRAME game
# export, and the files are byte-identical to the SDK's copies
# (personal/evidence/ef2-sdk/ef2-sdk/dlls/game).  Their port-side surface notes live in
# fgame/ai_main.h and at each adjusted call site.

# retail fgame/scriptvariable.cpp is the game's implementation; the MOHAA
# script/ copy must not be globbed into the game.
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*/script/scriptvariable\\.cpp$")
# The retail fgame copies of q_shared.c/q_math.c are superseded by the qcommon
# copies for the game module (see the note above GAME_MODULE_SHARED_SOURCES).
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*/fgame/q_(shared|math)\\.c$")
# fgame/q_mathsys.c (retail) is fully overlapped by qcommon/q_math.c
# (AnglesToAxis/AxisToAngles/AngleVectors/PerpendicularVector/MatrixMultiply) --
# linking both double-defined every one of them.
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*/fgame/q_mathsys\\.c$")

# THE LEXER AND GRAMMAR ARE GONE.  code/parser/ held MOHAA's flex scanner and bison
# grammar plus the parse tree they built; the game library no longer contains a script
# compiler at all.  Retail EF2's compiler is a hand-written recursive-descent parser in
# code/fgame/{lexer,compiler,program}.cpp, which is ordinary C++ and needs no generator.
# find_package(BISON)/find_package(FLEX) went with it -- see cmake/libraries/all.cmake.

set(GAME_BINARY_SOURCES)
set(GAME_QVM_SOURCES)

set(UI_SOURCES)

set(UI_BINARY_SOURCES)
set(UI_QVM_SOURCES)

# EF2: the game DLL uses the retail SDK implementations of the shared core
# classes (fgame/{str,class,listener,script}.cpp are picked up by the
# GAME_SOURCES glob below). Exclude the MOHAA corepp duplicates from the game
# so the retail fgame versions are the only definitions linked into game.so.
set(GAME_SCRIPT_SYSTEM_SOURCES ${SCRIPT_SYSTEM_SOURCES})
list(FILTER GAME_SCRIPT_SYSTEM_SOURCES EXCLUDE REGEX ".*/corepp/(class|listener|script|str)\\.cpp$")
# retail SDK versions of these live in fgame/ (picked up by the GAME_SOURCES glob)
list(FILTER GAME_SCRIPT_SYSTEM_SOURCES EXCLUDE REGEX ".*/script/scriptvariable\\.cpp$")
# corepp/lightclass.cpp is MOHAA's memory-accounting copy of retail's class.cpp
# globals (totalmemallocated / numclassesallocated / DisplayMemoryUsage are defined
# in fgame/class.cpp, retail-verbatim, as int).  Linking both double-defines the
# globals with conflicting types (int vs size_t).
list(FILTER GAME_SCRIPT_SYSTEM_SOURCES EXCLUDE REGEX ".*/corepp/lightclass\\.cpp$")

set(GAME_MODULE_SHARED_SOURCES
    ${SOURCE_DIR}/qcommon/q_math.c
    ${SOURCE_DIR}/qcommon/q_shared.c
    # Gameplay* classes: retail compiles the Shared/qcommon sources into the
    # game module (see the NOTE above GAME_SOURCES).  gameplayformulamanager.cpp
    # is GAME_DLL-gated in retail (game-only); gameplaydatabase.cpp compiles in
    # both binaries, gameplaymanager.cpp too (already in shared_sources.cmake
    # for the engine side).
    ${SOURCE_DIR}/qcommon/gameplaymanager.cpp
    ${SOURCE_DIR}/qcommon/gameplaydatabase.cpp
    ${SOURCE_DIR}/qcommon/gameplayformulamanager.cpp
    ${GAME_SCRIPT_SYSTEM_SOURCES}
)

# EF2: the fgame copies of q_shared.c / q_math.c (retail Ritual files) are NOT
# compiled into game.so.  The game module's C++ code compiles against the qcommon
# (Q3) q_shared.h / q_math.h declarations -- g_public.h is the Q3-ABI layer
# (gentity_t, usercmd_t, usereyes_t) until the engine-ABI alignment lands -- so the
# qcommon definitions are the consistent ones.  Compiling BOTH copies defined every
# shared symbol twice with conflicting signatures (int vs size_t), which broke the
# link under LTO.  The retail-only functions the game still calls (PlayerStat_*)
# live in qcommon/q_shared.c alongside the rest.
# cgame still builds against the MOHAA corepp headers/implementations for now,
# so it keeps most of SCRIPT_SYSTEM_SOURCES.  The four MOHAA-only members that used
# to be dropped from cgame.so here were corepp/con_timer.cpp (MOHAA script timing),
# corepp/delegate.cpp (MOHAA delegate plumbing), corepp/lz77.cpp (MOHAA archive
# compression) and corepp/mem_tempalloc.cpp (MOHAA MEM_TempAlloc).  Linking them
# exported 26 invented symbols that nothing in cgame.so or the engine resolves.
# R124-C: con_timer.cpp and lz77.cpp are now DELETED from the tree outright (zero
# retail presence in any of the three images, zero in the EF2 SDK; con_timer had no
# instance and lz77 no caller).
# R125-F: delegate.cpp is DELETED from the tree outright too -- "delegate" is 0 bytes
# in all three retail images, 0 lines in every retail symbol table and 0 in the EF2
# SDK, and its five MulticastDelegate members had no .Add() subscriber tree-wide, so
# the Entity/Player members, the three Execute() calls and the fgame/entity.h include
# went with it (see cmake/shared_script.cmake).  mem_tempalloc survives because
# tiki/tiki_files.cpp uses it as TIKI_allocator.
set(RESIDUE_CGAME_DROP mem_tempalloc.cpp)

# EF2: cgame compiles the RETAIL Event/Listener/Class/str/Script core, not the
# MOHAA corepp+script one.  Retail instances these files per module
# (personal/evidence/mac_symbols/pc_binary_source_paths.txt:24-27 -- dlls/cgame/{class,
# listener,script,str}.cpp vs dlls/game/{class,listener,script,str}.cpp:62-164;
# the SDK headers say it themselves, "instanced in each one of these
# directories"), and retail's cgame carries no ScriptVariable / ScriptException
# / MEM_BlockAlloc / con_set / LightClass symbols at all (cgamearch_0908.md
# section 3).  cgame's own copy lives in code/cgame/ and resolves the SDK's
# CGAME_DLL branches, so Event stores Container<EventVar> with the retail
# guard-and-return accessors instead of MOHAA's ScriptVariable* + throw.
#
# Everything below therefore stays OUT of cgame.so:
#   corepp/{class,listener,script}.cpp  -- replaced by code/cgame/*.cpp
#   script/{scriptvariable,scriptexception}.cpp -- ScriptVariable is gone
#   corepp/lightclass.cpp -- only reachable through the MOHAA Event/Class
# corepp/{str,con_set,mem_blockalloc}.cpp STAY in cgame.so: code/skeletor/
# skeletor.h hard-includes corepp/container.h and corepp/tiki.h hard-includes
# corepp/{mem_blockalloc,con_set,str}.h, so the MOHAA str/Container types are
# still the ones every cgame TU sees (see report, "not migrated and why").
set(CGAME_MODULE_SHARED_SOURCES
    ${SOURCE_DIR}/qcommon/q_math.c
    ${SOURCE_DIR}/qcommon/q_shared.c
    ${SOURCE_DIR}/cgame/class.cpp
    ${SOURCE_DIR}/cgame/listener.cpp
    ${SOURCE_DIR}/cgame/script.cpp
    # cgame's instance of retail's Shared/qcommon/output.cpp (DocFileOutput and
    # friends).  cgame_dll_symbols.txt:0x0005D610-0x0005F250 has them, and
    # cgame/class.cpp's CG_DumpClassEvents_f path constructs the two derived
    # formatters, so the definitions must be in this module.
    ${SOURCE_DIR}/cgame/output.cpp
    # the MOHAA str implementation: see the note above on skeletor.h / tiki.h
    ${SOURCE_DIR}/corepp/str.cpp
)

set(CGAME_SOURCES_BASEGAME ${CGAME_SOURCES} ${CGAME_MODULE_SHARED_SOURCES})
foreach(_rsrc IN LISTS RESIDUE_CGAME_DROP)
    list(FILTER CGAME_SOURCES_BASEGAME EXCLUDE REGEX ".*/${_rsrc}$")
endforeach()
set(GAME_SOURCES_BASEGAME ${GAME_SOURCES} ${GAME_MODULE_SHARED_SOURCES})
set(UI_SOURCES_BASEGAME ${UI_SOURCES} ${GAME_MODULE_SHARED_SOURCES})

if(BUILD_GAME_LIBRARIES)
    set(CGAME_MODULE_BINARY ${CGAME_MODULE})
    set(GAME_MODULE_BINARY ${GAME_MODULE})
    set(UI_MODULE_BINARY ${UI_MODULE})

    if (BASEGAME)
        set(CGAME_MODULE_BINARY_BASEGAME ${CGAME_MODULE_BINARY}_${BASEGAME})
        set(GAME_MODULE_BINARY_BASEGAME ${GAME_MODULE_BINARY}_${BASEGAME})
        set(UI_MODULE_BINARY_BASEGAME ${UI_MODULE_BINARY}_${BASEGAME})
    else()
        set(CGAME_MODULE_BINARY_BASEGAME ${CGAME_MODULE_BINARY})
        set(GAME_MODULE_BINARY_BASEGAME ${GAME_MODULE_BINARY})
        set(UI_MODULE_BINARY_BASEGAME ${UI_MODULE_BINARY})
    endif()

    add_library(                ${CGAME_MODULE_BINARY_BASEGAME} SHARED ${CGAME_SOURCES_BASEGAME} ${BG_SOURCES} ${CGAME_BINARY_SOURCES})
    target_compile_definitions( ${CGAME_MODULE_BINARY_BASEGAME} PRIVATE CGAME_DLL)
    target_link_libraries(      ${CGAME_MODULE_BINARY_BASEGAME} PRIVATE ${COMMON_LIBRARIES})
    set_target_properties(      ${CGAME_MODULE_BINARY_BASEGAME} PROPERTIES OUTPUT_NAME ${CGAME_MODULE_BINARY})
    set_output_dirs(            ${CGAME_MODULE_BINARY_BASEGAME} SUBDIRECTORY ${BASEGAME})

    INSTALL(TARGETS ${CGAME_MODULE_BINARY_BASEGAME} DESTINATION ${INSTALL_LIBDIR_FULL})

    if(MSVC)
        INSTALL(FILES $<TARGET_PDB_FILE:${CGAME_MODULE_BINARY_BASEGAME}> DESTINATION ${INSTALL_LIBDIR_FULL} OPTIONAL)
    endif()

    add_library(                ${GAME_MODULE_BINARY_BASEGAME} SHARED ${GAME_SOURCES_BASEGAME} ${BG_SOURCES} ${GAME_BINARY_SOURCES})
    target_compile_definitions( ${GAME_MODULE_BINARY_BASEGAME} PRIVATE GAME_DLL WITH_SCRIPT_ENGINE ARCHIVE_SUPPORTED)
    target_link_libraries(      ${GAME_MODULE_BINARY_BASEGAME} PRIVATE ${COMMON_LIBRARIES})
    set_target_properties(      ${GAME_MODULE_BINARY_BASEGAME} PROPERTIES OUTPUT_NAME ${GAME_MODULE_BINARY})
    set_output_dirs(            ${GAME_MODULE_BINARY_BASEGAME} SUBDIRECTORY ${BASEGAME})

    INSTALL(TARGETS ${GAME_MODULE_BINARY_BASEGAME} DESTINATION ${INSTALL_LIBDIR_FULL})

    if(MSVC)
        INSTALL(FILES $<TARGET_PDB_FILE:${GAME_MODULE_BINARY_BASEGAME}> DESTINATION ${INSTALL_LIBDIR_FULL} OPTIONAL)
    endif()

    #
    # LTO is disabled for the script modules.  Their global Event definitions
    # self-register: Event::Event stores `this` into Event::eventDefList.  GCC
    # LTO's partitioned static initializers construct those globals into a
    # REUSED STACK SLOT and memcpy them to their .bss homes, so eventDefList
    # ended up holding ~894 dead temporaries and Event::SetupDocumentation
    # crashed on the first map start.  Retail (CodeWarrior PPC) constructs the
    # globals in place; without LTO both GCC and the future Android NDK
    # toolchain do the same.  The engine targets keep LTO.
    #
    set_target_properties(      ${CGAME_MODULE_BINARY_BASEGAME} ${GAME_MODULE_BINARY_BASEGAME}
                                PROPERTIES INTERPROCEDURAL_OPTIMIZATION FALSE)

    #add_library(                ${UI_MODULE_BINARY_BASEGAME} SHARED ${UI_SOURCES_BASEGAME} ${UI_BINARY_SOURCES})
    #target_compile_definitions( ${UI_MODULE_BINARY_BASEGAME} PRIVATE UI)
    #set_target_properties(      ${UI_MODULE_BINARY_BASEGAME} PROPERTIES OUTPUT_NAME ${UI_MODULE_BINARY})
    #set_output_dirs(            ${UI_MODULE_BINARY_BASEGAME} SUBDIRECTORY ${BASEGAME})
endif()
