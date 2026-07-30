#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************

# VinaText — cross-platform build.
#
# Scope today: core/ and its tests. This does NOT build the MFC application;
# that stays on src/VinaText.vcxproj and MSBuild until Phase 6 cutover, and the
# Windows CI job keeps building it exactly as before. Nothing here can affect it.
#
# See doc/QT-PORT-BRIEF.md:
#   D5  core/ is UI-free and portable, and links into both frontends
#   D3  Qt is LGPLv3 and must be linked DYNAMICALLY — never static
#
# Usage:
#   cmake -S . -B build -G Ninja
#   cmake --build build
#   ctest --test-dir build --output-on-failure

# 3.24, not 3.21: thirdparty/CMakeLists.txt uses FetchContent's
# DOWNLOAD_EXTRACT_TIMESTAMP, which was added in 3.24 (policy CMP0135).
cmake_minimum_required(VERSION 3.24)

project(VinaText
    VERSION 1.17.0
    DESCRIPTION "VinaText — a lightweight text and source editor"
    HOMEPAGE_URL "https://github.com/vinadevs/VinaText"
    LANGUAGES CXX)

#-------------------------------------------------------------------------------
# Options
#-------------------------------------------------------------------------------

# Off by default so this file can never break anyone's build by existing.
# Phase 3 turns it on to build the Qt frontend in ui-qt/.
option(VINATEXT_BUILD_QT "Build the Qt frontend (ui-qt/)" OFF)
option(VINATEXT_BUILD_TESTS "Build the core/ tests" ON)

#-------------------------------------------------------------------------------
# Language level
#-------------------------------------------------------------------------------

# The MFC project is <LanguageStandard>stdcpp14</LanguageStandard>. core/ is
# deliberately held to C++11 so it stays linkable from that project without
# forcing a standard bump on the shipping app.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

#-------------------------------------------------------------------------------
# core/ — UI-free, portable
#-------------------------------------------------------------------------------

add_library(vinatext_core STATIC
    core/LanguageData.cpp
    core/LanguageData.h
    core/StringUtil.h
    core/Checksum.cpp
    core/Checksum.h
    core/TextTransform.h
    core/Tokenizer.cpp
    core/Tokenizer.h)

add_library(VinaText::core ALIAS vinatext_core)

target_include_directories(vinatext_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include)   # vendored picojson lives here

# Warnings are errors here and nowhere else. core/ is new code held to a
# standard the 2013-era MFC tree cannot meet, and this is the wall that stops
# Win32 or MFC dependencies creeping back in — a stray platform API fails the
# Linux and macOS builds immediately rather than at Phase 3.
if(MSVC)
    target_compile_options(vinatext_core PRIVATE /W4 /WX)
else()
    target_compile_options(vinatext_core PRIVATE -Wall -Wextra -Werror)
endif()

#-------------------------------------------------------------------------------
# Tests
#-------------------------------------------------------------------------------

if(VINATEXT_BUILD_TESTS)
    enable_testing()

    # No test framework on purpose: these must build anywhere a C++11 compiler
    # exists, with no package manager and no network. Each returns non-zero on
    # failure, which is all ctest needs.
    foreach(test_name TestLanguageData TestStringUtil TestChecksum TestTextTransform TestTokenizer)
        add_executable(${test_name} core/tests/${test_name}.cpp)
        target_link_libraries(${test_name} PRIVATE vinatext_core)
        if(MSVC)
            target_compile_options(${test_name} PRIVATE /W4 /WX)
        else()
            target_compile_options(${test_name} PRIVATE -Wall -Wextra -Werror)
        endif()
    endforeach()

    # TestLanguageData reads the shipped JSON, so it runs from the source tree.
    add_test(NAME core.LanguageData
             COMMAND TestLanguageData "${CMAKE_CURRENT_SOURCE_DIR}/Packages/data-packages")
    add_test(NAME core.StringUtil COMMAND TestStringUtil)
    add_test(NAME core.Checksum COMMAND TestChecksum)
    add_test(NAME core.TextTransform COMMAND TestTextTransform)
    add_test(NAME core.Tokenizer COMMAND TestTokenizer)
endif()

#-------------------------------------------------------------------------------
# Qt frontend — Phase 3
#-------------------------------------------------------------------------------

if(VINATEXT_BUILD_QT)
    add_subdirectory(ui-qt)
endif()

#-------------------------------------------------------------------------------

message(STATUS "VinaText ${PROJECT_VERSION}")
message(STATUS "  core/       ON  (C++${CMAKE_CXX_STANDARD}, warnings are errors)")
message(STATUS "  tests       ${VINATEXT_BUILD_TESTS}")
message(STATUS "  Qt frontend ${VINATEXT_BUILD_QT}")
message(STATUS "  MFC app     built by src/VinaText.vcxproj, not by CMake")
