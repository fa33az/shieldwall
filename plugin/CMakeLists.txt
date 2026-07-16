cmake_minimum_required(VERSION 3.15)
project(shieldwall C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch SA-MP SDK
include(FetchContent)
FetchContent_Declare(
    samp-plugin-sdk
    GIT_REPOSITORY https://github.com/maddinat0r/samp-plugin-sdk.git
    GIT_TAG        master
)
FetchContent_GetProperties(samp-plugin-sdk)
if(NOT samp-plugin-sdk_POPULATED)
    FetchContent_Populate(samp-plugin-sdk)
endif()

find_package(Threads REQUIRED)

add_library(shieldwall SHARED
    src/main.cpp
    src/logger.cpp
    src/database.cpp
    src/rate_limiter.cpp
    src/raknet_hook.cpp
    src/sqlite/sqlite3.c
    ${samp-plugin-sdk_SOURCE_DIR}/amxplugin.cpp
)

target_include_directories(shieldwall PRIVATE
    ${samp-plugin-sdk_SOURCE_DIR}
    src
    src/sqlite
)

target_link_libraries(shieldwall PRIVATE
    Threads::Threads
)

target_compile_definitions(shieldwall PRIVATE HAVE_STDINT_H)

if(MSVC)
    target_compile_definitions(shieldwall PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN)
    target_compile_definitions(shieldwall PRIVATE SQLITE_THREADSAFE=1)
else()
    target_compile_options(shieldwall PRIVATE -fvisibility=hidden)
    target_compile_definitions(shieldwall PRIVATE SQLITE_THREADSAFE=1)
endif()
