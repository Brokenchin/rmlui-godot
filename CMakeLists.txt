cmake_minimum_required(VERSION 3.20)
project(rmlui-godot LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- Options ---
option(RMLUI_GODOT_STANDALONE "Build as standalone shared library (.dll/.so/.dylib)" ON)

# --- Godot project root & addon install paths ---
# User sets GODOT_PROJECT_ROOT to wherever project.godot lives.
# All addon paths are derived from it following Godot conventions.
set(GODOT_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/project" CACHE PATH
    "Godot project directory (where project.godot lives)")
set(RMLUI_GODOT_ADDON_NAME "rmlui-godot" CACHE STRING
    "Plugin folder name under addons/")

if(RMLUI_GODOT_STANDALONE)
    option(RMLUI_GODOT_INSTALL_EXAMPLES "Fetch and install examples into addon directory" ON)
else()
    option(RMLUI_GODOT_INSTALL_EXAMPLES "Fetch and install examples into addon directory" OFF)
endif()

if(NOT EXISTS "${GODOT_PROJECT_ROOT}")
    message(FATAL_ERROR
        "GODOT_PROJECT_ROOT does not exist: ${GODOT_PROJECT_ROOT}\n"
        "  Set -DGODOT_PROJECT_ROOT=<path> to the directory containing project.godot")
endif()

set(RMLUI_ADDON_INSTALL_DIR "${GODOT_PROJECT_ROOT}/addons/${RMLUI_GODOT_ADDON_NAME}")
set(RMLUI_ADDON_SOURCE_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/project/addons/${RMLUI_GODOT_ADDON_NAME}")

# --- Examples submodule (opt-in) ---
if(RMLUI_GODOT_INSTALL_EXAMPLES)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt"
       AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/README.md")
        find_package(Git QUIET)
        if(Git_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.gitmodules")
            message(STATUS "Fetching rmlui-godot-examples submodule...")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} submodule update --init examples
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                RESULT_VARIABLE _examples_result
            )
            if(NOT _examples_result EQUAL 0)
                message(WARNING
                    "Failed to fetch examples submodule. Examples will not be installed.\n"
                    "  Run manually: git submodule update --init examples")
                set(RMLUI_GODOT_INSTALL_EXAMPLES OFF)
            endif()
        else()
            message(WARNING
                "Examples submodule not found and git not available. Examples will not be installed.\n"
                "  Clone https://github.com/Brokenchin/rmlui-godot-examples into examples/")
            set(RMLUI_GODOT_INSTALL_EXAMPLES OFF)
        endif()
    endif()
endif()

# --- Dependencies ---
# Both godot-cpp and RmlUi are available as submodules under dependencies/.
# When embedded in a host project (e.g. CaveCrawler), the host provides these
# targets already — the NOT TARGET check skips everything in that case.
# Priority: existing target → explicit *_DIR → submodule → error with fetch hint.

# godot-cpp
if(NOT TARGET godot-cpp)
    if(DEFINED GODOT_CPP_DIR)
        add_subdirectory(${GODOT_CPP_DIR} godot-cpp)
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/godot-cpp/CMakeLists.txt")
        add_subdirectory(dependencies/godot-cpp)
    else()
        find_package(Git QUIET)
        if(Git_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.gitmodules")
            message(STATUS "Fetching godot-cpp submodule...")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} submodule update --init dependencies/godot-cpp
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                RESULT_VARIABLE _godot_cpp_result
            )
            if(_godot_cpp_result EQUAL 0 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/godot-cpp/CMakeLists.txt")
                add_subdirectory(dependencies/godot-cpp)
            else()
                message(FATAL_ERROR "Failed to fetch godot-cpp submodule. Run manually:\n"
                    "  git submodule update --init dependencies/godot-cpp")
            endif()
        else()
            message(FATAL_ERROR
                "godot-cpp not found. Either:\n"
                "  - Run: git submodule update --init dependencies/godot-cpp\n"
                "  - Set GODOT_CPP_DIR to your godot-cpp checkout")
        endif()
    endif()
endif()

# RmlUi (Brokenchin/RmlUi-multicontext fork — adds ReleaseRenderManager API)
# Force static so RmlUi gets linked into our shared lib (no separate DLLs to ship).
if(NOT TARGET rmlui_core)
    set(_PREV_BUILD_SHARED ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF)
    set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
    set(RMLUI_FONT_ENGINE "none" CACHE STRING "" FORCE)
    set(RMLUI_LUA_BINDINGS OFF CACHE BOOL "" FORCE)
    set(RMLUI_LOTTIE_PLUGIN OFF CACHE BOOL "" FORCE)
    set(RMLUI_SVG_PLUGIN OFF CACHE BOOL "" FORCE)
    set(RMLUI_CUSTOM_RTTI ON CACHE BOOL "" FORCE)
    if(DEFINED RMLUI_DIR)
        add_subdirectory(${RMLUI_DIR} rmlui)
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/RmlUi/CMakeLists.txt")
        add_subdirectory(dependencies/RmlUi)
    else()
        find_package(Git QUIET)
        if(Git_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.gitmodules")
            message(STATUS "Fetching RmlUi submodule...")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} submodule update --init dependencies/RmlUi
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                RESULT_VARIABLE _rmlui_result
            )
            if(_rmlui_result EQUAL 0 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/RmlUi/CMakeLists.txt")
                add_subdirectory(dependencies/RmlUi)
            else()
                message(FATAL_ERROR "Failed to fetch RmlUi submodule. Run manually:\n"
                    "  git submodule update --init dependencies/RmlUi")
            endif()
        else()
            message(FATAL_ERROR
                "RmlUi not found. Either:\n"
                "  - Run: git submodule update --init dependencies/RmlUi\n"
                "  - Set RMLUI_DIR to your RmlUi checkout")
        endif()
    endif()
    set(BUILD_SHARED_LIBS ${_PREV_BUILD_SHARED})
endif()

# --- Collect sources ---
file(GLOB_RECURSE RMLUI_GODOT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rmlui_godot/*.cpp"
)
file(GLOB_RECURSE RMLUI_GODOT_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rmlui_godot/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rmlui_godot/*.h"
)


# --- Library target ---
if(RMLUI_GODOT_STANDALONE)
    add_library(rmlui_godot SHARED ${RMLUI_GODOT_SOURCES} ${RMLUI_GODOT_HEADERS})
    target_compile_definitions(rmlui_godot PRIVATE RMLUI_GODOT_STANDALONE=1)
else()
    add_library(rmlui_godot STATIC ${RMLUI_GODOT_SOURCES} ${RMLUI_GODOT_HEADERS})
endif()

target_include_directories(rmlui_godot PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(rmlui_godot
    PUBLIC godot-cpp rmlui_core rmlui_debugger
)

# godot-cpp propagates -fno-exceptions. RmlUi's itlib/flat_map.hpp uses
# throw in at() — macOS Clang hard-errors on throw with exceptions disabled.
# We never call at(), so switch itlib to assert-based error handling.
target_compile_definitions(rmlui_godot PRIVATE ITLIB_FLAT_MAP_NO_THROW)

# --- Output naming ---
# Single-config generators (Ninja, Make): uses CMAKE_BUILD_TYPE to pick suffix.
# Multi-config generators (MSVC): uses per-config OUTPUT_NAME via generator expressions.
# Override with -DRMLUI_OUTPUT_SUFFIX=".custom" to force a specific suffix.
if(RMLUI_GODOT_STANDALONE)
    set(_BIN_DIR "${RMLUI_ADDON_INSTALL_DIR}/bin")

    get_property(_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

    if(_is_multi_config)
        set_target_properties(rmlui_godot PROPERTIES
            PREFIX ""
            OUTPUT_NAME_DEBUG "rmlui_godot.debug"
            OUTPUT_NAME_RELEASE "rmlui_godot.release"
            OUTPUT_NAME_RELWITHDEBINFO "rmlui_godot.debug"
            OUTPUT_NAME_MINSIZEREL "rmlui_godot.release"
            RUNTIME_OUTPUT_DIRECTORY "${_BIN_DIR}"
            LIBRARY_OUTPUT_DIRECTORY "${_BIN_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${_BIN_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${_BIN_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${_BIN_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${_BIN_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_DEBUG "${_BIN_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_RELEASE "${_BIN_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${_BIN_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${_BIN_DIR}"
        )
    else()
        if(NOT DEFINED RMLUI_OUTPUT_SUFFIX)
            string(TOLOWER "${CMAKE_BUILD_TYPE}" _build_lower)
            if(_build_lower STREQUAL "debug" OR _build_lower STREQUAL "relwithdebinfo")
                set(RMLUI_OUTPUT_SUFFIX ".debug")
            else()
                set(RMLUI_OUTPUT_SUFFIX ".release")
            endif()
        endif()
        set_target_properties(rmlui_godot PROPERTIES
            PREFIX ""
            OUTPUT_NAME "rmlui_godot${RMLUI_OUTPUT_SUFFIX}"
            RUNTIME_OUTPUT_DIRECTORY "${_BIN_DIR}"
            LIBRARY_OUTPUT_DIRECTORY "${_BIN_DIR}"
        )
    endif()
endif()

# --- Addon install ---
# Standalone: copy addon files (plugin.cfg, base.rcss, .gdextension, editor/) to install dir.
# When GODOT_PROJECT_ROOT differs from the in-repo project/, files are synced post-build.
# When they're the same path, the copy is skipped (already in place).
if(RMLUI_GODOT_STANDALONE)
    cmake_path(ABSOLUTE_PATH RMLUI_ADDON_INSTALL_DIR NORMALIZE OUTPUT_VARIABLE _install_norm)
    cmake_path(ABSOLUTE_PATH RMLUI_ADDON_SOURCE_DIR  NORMALIZE OUTPUT_VARIABLE _source_norm)

    if(NOT _install_norm STREQUAL _source_norm)
        set(_ADDON_FILES
            plugin.cfg
            base.rcss
            rmlui_godot.gdextension
        )
        foreach(_file IN LISTS _ADDON_FILES)
            if(EXISTS "${RMLUI_ADDON_SOURCE_DIR}/${_file}")
                add_custom_command(TARGET rmlui_godot POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${RMLUI_ADDON_SOURCE_DIR}/${_file}"
                        "${RMLUI_ADDON_INSTALL_DIR}/${_file}"
                    COMMENT "Installing ${_file} → ${RMLUI_ADDON_INSTALL_DIR}"
                )
            endif()
        endforeach()

        if(EXISTS "${RMLUI_ADDON_SOURCE_DIR}/editor")
            add_custom_command(TARGET rmlui_godot POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${RMLUI_ADDON_SOURCE_DIR}/editor"
                    "${RMLUI_ADDON_INSTALL_DIR}/editor"
                COMMENT "Installing editor/ → ${RMLUI_ADDON_INSTALL_DIR}/editor"
            )
        endif()
    endif()

    if(RMLUI_GODOT_INSTALL_EXAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples")
        add_custom_command(TARGET rmlui_godot POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}/examples
                -DDST_DIR=${RMLUI_ADDON_INSTALL_DIR}/examples
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sync_examples.cmake
            COMMENT "Syncing examples → ${RMLUI_ADDON_INSTALL_DIR}/examples"
        )
endif()
endif()

# --- Embedded addon install ---
# When building as static lib for a host project, copy only the files Godot needs
# to discover the editor plugin: plugin.cfg, base.rcss, editor/*.gd.
# Deliberately skip .gdextension and bin/ — the C++ is statically linked by the
# host, so Godot must not try to load a standalone GDExtension binary.
if(NOT RMLUI_GODOT_STANDALONE)
    cmake_path(ABSOLUTE_PATH RMLUI_ADDON_INSTALL_DIR NORMALIZE OUTPUT_VARIABLE _install_norm)
    cmake_path(ABSOLUTE_PATH RMLUI_ADDON_SOURCE_DIR  NORMALIZE OUTPUT_VARIABLE _source_norm)

    if(NOT _install_norm STREQUAL _source_norm)
        file(MAKE_DIRECTORY "${RMLUI_ADDON_INSTALL_DIR}")

        set(_EMBEDDED_ADDON_FILES plugin.cfg base.rcss)
        foreach(_file IN LISTS _EMBEDDED_ADDON_FILES)
            if(EXISTS "${RMLUI_ADDON_SOURCE_DIR}/${_file}")
                file(COPY_FILE
                    "${RMLUI_ADDON_SOURCE_DIR}/${_file}"
                    "${RMLUI_ADDON_INSTALL_DIR}/${_file}"
                    ONLY_IF_DIFFERENT)
            endif()
        endforeach()

        if(EXISTS "${RMLUI_ADDON_SOURCE_DIR}/editor")
            file(COPY "${RMLUI_ADDON_SOURCE_DIR}/editor"
                DESTINATION "${RMLUI_ADDON_INSTALL_DIR}")
        endif()

        message(STATUS "Installed embedded addon files → ${RMLUI_ADDON_INSTALL_DIR} (no .gdextension)")
    endif()
endif()
