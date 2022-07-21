cmake_minimum_required (VERSION 2.8)

project (demo)

set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(client)
add_subdirectory(server)
