include_guard(GLOBAL)

include(utils/add_git_dependency)
include(utils/disable_warnings)
include(gamespy)

set(COMMON_SOURCES
    ${SOURCE_DIR}/qcommon/alias.c
    ${SOURCE_DIR}/qcommon/cm_load.c
    ${SOURCE_DIR}/qcommon/cm_patch.c
    ${SOURCE_DIR}/qcommon/cm_polylib.c
    ${SOURCE_DIR}/qcommon/cm_terrain_ef2.c
    ${SOURCE_DIR}/qcommon/cm_test.c
    ${SOURCE_DIR}/qcommon/cm_trace.c
    ${SOURCE_DIR}/qcommon/cmd.c
    ${SOURCE_DIR}/qcommon/common.c
    ${SOURCE_DIR}/qcommon/crc.c
    ${SOURCE_DIR}/qcommon/cvar.c
    ${SOURCE_DIR}/qcommon/files.cpp
    ${SOURCE_DIR}/qcommon/gameplaymanager.cpp
    ${SOURCE_DIR}/qcommon/gameplaydatabase.cpp
    ${SOURCE_DIR}/qcommon/ioapi.c
    ${SOURCE_DIR}/qcommon/lip_sync.c
    ${SOURCE_DIR}/qcommon/huffman.cpp
    ${SOURCE_DIR}/qcommon/md4.c
    ${SOURCE_DIR}/qcommon/md5.c
    ${SOURCE_DIR}/qcommon/memory.c
    ${SOURCE_DIR}/qcommon/msg.cpp
    ${SOURCE_DIR}/qcommon/net_chan.c
    ${SOURCE_DIR}/qcommon/net_ip.c
    ${SOURCE_DIR}/qcommon/q_math.c
    ${SOURCE_DIR}/qcommon/q_shared.c
    ${SOURCE_DIR}/qcommon/unzip.c
    ${SOURCE_DIR}/gamespy/q_gamespy.c
)

disable_warnings(
    ${SOURCE_DIR}/qcommon/unzip.c
    ${SOURCE_DIR}/qcommon/ioapi.c
)

add_git_dependency(${SOURCE_DIR}/qcommon/common.c)

set(SYSTEM_SOURCES
    ${SOURCE_DIR}/sys/con_log.c
    ${SOURCE_DIR}/sys/sys_autoupdater.c
    ${SOURCE_DIR}/sys/sys_main.c
    ${SYSTEM_PLATFORM_SOURCES}
)

set(SERVER_SOURCES
    ${SOURCE_DIR}/server/sv_client.c
    ${SOURCE_DIR}/server/sv_ccmds.c
    ${SOURCE_DIR}/server/sv_game.c
    ${SOURCE_DIR}/server/sv_init.c
    ${SOURCE_DIR}/server/sv_main.c
    ${SOURCE_DIR}/server/sv_net_chan.c
    ${SOURCE_DIR}/server/sv_snapshot.c
    ${SOURCE_DIR}/server/sv_snd.c
    ${SOURCE_DIR}/server/sv_world.c
)

list(APPEND SYSTEM_SOURCES
    ${SOURCE_DIR}/sys/new/sys_main_new.c
    ${SOURCE_DIR}/sys/win_bounds.cpp
)

# EF2 botlib (Q3-derived, shipped in the retail engine, not in game.so).
# Retail reaches it through the AAS_/EA_/Bot*/PC_* slots of game_import_t
# (SDK g_public.h:434-699), all of which are PF wrappers in code/server.
# botlib.h is not duplicated here -- code/botlib/botlib.h is a shim onto
# code/fgame/botlib.h, which is the SDK's dlls/game/botlib.h verbatim.
file(GLOB BOTLIB_SOURCES ${SOURCE_DIR}/botlib/*.c)
list(APPEND COMMON_SOURCES ${BOTLIB_SOURCES})
# The engine side of the library: botlib_import_t callbacks plus the forwarders
# that game_import_t's AAS_/EA_/Bot*/PC_* slots bind to (see SV_BotInitImport in
# server/sv_game.c).
list(APPEND SERVER_SOURCES ${SOURCE_DIR}/server/sv_bot.c)
include_directories(${SOURCE_DIR}/botlib)

# Append TIKI sources

list(APPEND COMMON_SOURCES
    ${SOURCE_DIR}/skeletor/bonetable.cpp
    ${SOURCE_DIR}/skeletor/skeletor_imports.cpp
    ${SOURCE_DIR}/skeletor/skeletor_loadanimation.cpp
    ${SOURCE_DIR}/skeletor/skeletor_model_files.cpp
    ${SOURCE_DIR}/skeletor/skeletor_utilities.cpp
    ${SOURCE_DIR}/skeletor/skeletor.cpp
    ${SOURCE_DIR}/skeletor/skeletorbones.cpp
    ${SOURCE_DIR}/skeletor/tokenizer.cpp
	${SOURCE_DIR}/tiki/tiki_anim.cpp
	${SOURCE_DIR}/tiki/tiki_cache.cpp
	${SOURCE_DIR}/tiki/tiki_commands.cpp
	${SOURCE_DIR}/tiki/tiki_files.cpp
	${SOURCE_DIR}/tiki/tiki_frame.cpp
	${SOURCE_DIR}/tiki/tiki_imports.cpp
	${SOURCE_DIR}/tiki/tiki_parse.cpp
	${SOURCE_DIR}/tiki/tiki_skel.cpp
${SOURCE_DIR}/tiki/tiki_skan.cpp
	${SOURCE_DIR}/tiki/tiki_surface.cpp
	${SOURCE_DIR}/tiki/tiki_tag.cpp
	${SOURCE_DIR}/tiki/tiki_utility.cpp
	${SOURCE_DIR}/corepp/tiki_main.cpp
	${SOURCE_DIR}/corepp/tiki_script.cpp
)

# Gamespy server responder: EF2 retail has NO gamespy query responder in the
# engine (no SV_ProcessGamespyQueries / SV_GamespyHeartbeat / "authorizeThis"
# anywhere in the 4,802-function Mac decompile).  The client-side SDK stays
# (retail's in-game server browser is engine-native; the port's UIFAKKServerList
# still compiles against the vendored SDK until the client workstream rewrites it).

include_directories(${SOURCE_DIR}/qcommon)
include_directories(${SOURCE_DIR}/script)
include_directories(${SOURCE_DIR}/corepp)
include_directories(${SOURCE_DIR})
