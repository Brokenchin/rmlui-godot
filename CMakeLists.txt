cmake_minimum_required(VERSION 3.20)
project(rmlui-godot LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- Options ---
option(RMLUI_GODOT_STANDALONE "Build as standalone shared library (.dll/.so/.dylib)" ON)

# --- Dependencies ---
# godot-cpp: set GODOT_CPP_DIR or add as subdirectory
if(NOT TARGET godot-cpp)
    if(DEFINED GODOT_CPP_DIR)
        add_subdirectory(${GODOT_CPP_DIR} godot-cpp)
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/godot-cpp/CMakeLists.txt")
        add_subdirectory(dependencies/godot-cpp)
    else()
        message(FATAL_ERROR
            "godot-cpp not found. Either:\n"
            "  - Set GODOT_CPP_DIR to your godot-cpp checkout\n"
            "  - Clone godot-cpp into dependencies/godot-cpp\n"
            "  - Use CPM/FetchContent to pull it")
    endif()
endif()

# RmlUi: set RMLUI_DIR or add as subdirectory
if(NOT TARGET rmlui_core)
    if(DEFINED RMLUI_DIR)
        add_subdirectory(${RMLUI_DIR} rmlui)
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/RmlUi/CMakeLists.txt")
        add_subdirectory(dependencies/RmlUi)
    else()
        message(FATAL_ERROR
            "RmlUi not found. Either:\n"
            "  - Set RMLUI_DIR to your RmlUi checkout\n"
            "  - Clone RmlUi into dependencies/RmlUi\n"
            "  - Use CPM/FetchContent to pull it")
    endif()
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
    PUBLIC godot-cpp rmlui_core
)

# --- Output naming ---
# Produces: rmlui_godot.debug.dll / rmlui_godot.release.dll (etc.)
# Override with -DRMLUI_OUTPUT_SUFFIX=".custom" for non-standard names.
if(NOT DEFINED RMLUI_OUTPUT_SUFFIX)
    string(TOLOWER "${CMAKE_BUILD_TYPE}" _build_lower)
    if(_build_lower STREQUAL "debug" OR _build_lower STREQUAL "relwithdebinfo")
        set(RMLUI_OUTPUT_SUFFIX ".debug")
    else()
        set(RMLUI_OUTPUT_SUFFIX ".release")
    endif()
endif()

if(RMLUI_GODOT_STANDALONE)
    set_target_properties(rmlui_godot PROPERTIES
        PREFIX ""
        OUTPUT_NAME "rmlui_godot${RMLUI_OUTPUT_SUFFIX}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/project/addons/rmlui-godot/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/project/addons/rmlui-godot/bin"
    )
endif()
