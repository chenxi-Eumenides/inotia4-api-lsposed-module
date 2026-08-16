cmake_minimum_required(VERSION 3.22.1)
project(gamebridge)

add_library(gamebridge SHARED
    gamebridge.cpp
    game_access.cpp
    symbol_resolver.cpp
    game_json.cpp
    game_ops_common.cpp
    game_state.cpp
    game_nav.cpp
    game_read.cpp
    game_cache.cpp
    game_world.cpp
    game_quest.cpp
    game_ui.cpp
    game_dialog.cpp
    game_shop.cpp
    game_save.cpp
    game_system.cpp
    game_motion.cpp
    game_tiles.cpp
    game_character.cpp
    game_party.cpp
    game_inventory.cpp
    game_patch.cpp
)

find_library(log-lib log)
target_link_libraries(gamebridge ${log-lib} dl)
