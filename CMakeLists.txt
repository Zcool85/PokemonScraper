cmake_minimum_required(VERSION 3.25)
project(PokemonScraper)

set(CMAKE_CXX_STANDARD 20)

# fetch latest rapidjson
include(FetchContent)
FetchContent_Declare(
        rapidjson
        GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
        GIT_TAG v1.1.0
)
set(RAPIDJSON_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_CXX11 OFF)
set(RAPIDJSON_BUILD_CXX17 OFF)
set(RAPIDJSON_BUILD_CXX20 ON)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(rapidjson)

message(STATUS "RapidJSON source dir: ${rapidjson_SOURCE_DIR}")

FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt
        GIT_TAG        12.1.0
)
FetchContent_MakeAvailable(fmt)

find_package(SQLite3 REQUIRED)

find_package(CURL REQUIRED)

add_executable(PokemonScraper main.cpp
        DatabaseManager.cpp
        DatabaseManager.h
        DownloadManager.cpp
        DownloadManager.h)

target_include_directories(PokemonScraper PRIVATE
        ${rapidjson_SOURCE_DIR}/include
)

target_link_libraries(PokemonScraper PRIVATE
        SQLite::SQLite3
        CURL::libcurl
        fmt::fmt
)
