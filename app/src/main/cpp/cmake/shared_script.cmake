include_guard(GLOBAL)

set(SCRIPT_SYSTEM_SOURCES
	${SOURCE_DIR}/corepp/class.cpp
	${SOURCE_DIR}/corepp/con_set.cpp
	${SOURCE_DIR}/corepp/lightclass.cpp
	${SOURCE_DIR}/corepp/listener.cpp
	${SOURCE_DIR}/corepp/mem_blockalloc.cpp
	${SOURCE_DIR}/corepp/mem_tempalloc.cpp
	${SOURCE_DIR}/corepp/script.cpp
	${SOURCE_DIR}/corepp/str.cpp
	${SOURCE_DIR}/script/scriptexception.cpp
	${SOURCE_DIR}/script/scriptvariable.cpp
)

# R124-C: corepp/con_timer.cpp and corepp/lz77.cpp are GONE from this list and from the
# tree.  Both are MOHAA modules with zero retail presence -- "con_timer" and "lz77"
# occur in zero mangled names, zero decompile units and zero strings in game_mac.pef,
# cgamex86-equivalent cgame_mac.pef and EF2_mac_engine.pef, and neither exists in the
# EF2 SDK's dlls/game.  con_timer had no instance anywhere in the tree (its only
# reference was the now-removed #include in fgame/scriptmaster.h) and lz77's only
# caller was its own [[maybe_unused]] self-test.  They were still being compiled into
# every target through this list.
#
# R125-F: corepp/delegate.cpp and corepp/delegate.h are GONE the same way.  The
# MulticastDelegate / DelegateMultiElement / DelegateHandle machinery is OpenMoHAA
# 2025 plumbing: "delegate" is 0 bytes in game_mac.pef, cgame_mac.pef and
# EF2_mac_engine.pef, 0 lines in game_dll_symbols.txt / cgame_dll_symbols.txt /
# engine_EF2exe_symbols.txt / every *_classes.txt, and 0 in the EF2 SDK's dlls/game
# (the sole "delegate" in the SDK is an English comment in vehicle.cpp:1567).  The
# five MulticastDelegate members (Entity::delegate_damage/_killed/_gotKill,
# Player::delegate_stufftext/_spawned) had no .Add() subscriber anywhere in the
# tree, so every Execute() iterated an empty Container; the members, the Execute()
# calls and the include in fgame/entity.h are deleted with it.
