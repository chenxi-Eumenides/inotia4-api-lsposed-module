cmake_minimum_required(VERSION 3.22.1)
project(gamebridge)

add_library(gamebridge SHARED
    gamebridge.cpp
    game_access.cpp
    game_data.cpp
)

find_library(log-lib log)
target_link_libraries(gamebridge ${log-lib} dl)
