cmake_minimum_required(VERSION 3.22.1)
project(gamebridge)

add_library(gamebridge SHARED
    gamebridge.cpp
    game_access.cpp
    symbol_resolver.cpp
    game_json.cpp
    game_state.cpp
    game_nav.cpp
    game_read.cpp
    game_cache.cpp
    game_misc.cpp
    game_motion.cpp
    game_tiles.cpp
    game_ops_value.cpp
    game_ops_action.cpp
)

find_library(log-lib log)
target_link_libraries(gamebridge ${log-lib} dl)
