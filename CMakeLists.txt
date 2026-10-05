# =============================================================================
# Energy Crisis - CMake build (C++17 + SFML 3)
#
#   cmake -S . -B build                         # uses an installed SFML 3
#   cmake -S . -B build -DEC_FETCH_SFML=ON      # downloads and builds SFML 3.1.0 if none is installed
#   cmake -S . -B build -DEC_BUILD_GAME=OFF     # engine + tests only, no SFML needed
#   cmake --build build --parallel
#   ctest --test-dir build --output-on-failure
#
# Targets:
#   ec_engine      static library with the game rules (Game/scr/*.cpp), no drawing
#   energy_crisis  the game (main.cpp + UI/scr/*.cpp), written to <build>/bin with assets/ next to it
#   test_*         one headless test program per scratch/test_*.cpp (links ec_engine only)
#   run            builds and starts the game:  cmake --build build --target run
#
# See docs/BUILD.md for full instructions (in Bulgarian).
# =============================================================================
cmake_minimum_required(VERSION 3.22...3.31)

project(EnergyCrisis
    VERSION 1.0.0
    DESCRIPTION "Energy Crisis - local 1v1 energy strategy game"
    LANGUAGES CXX)

option(EC_BUILD_GAME  "Build the game executable (needs SFML 3). OFF builds only ec_engine and the tests" ON)
option(EC_FETCH_SFML  "Download and build SFML with FetchContent when no installed SFML 3 is found" OFF)
option(EC_BUILD_TESTS "Build the headless engine tests (scratch/test_*.cpp) and register them with CTest" ON)
set(EC_SFML_VERSION "3.1.0" CACHE STRING "SFML release (git tag) downloaded when EC_FETCH_SFML is ON")
set(EC_TEST_TIMEOUT 120 CACHE STRING "Timeout in seconds for every engine test")

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The game, its DLLs and assets/ end up together in <build>/bin; test programs in <build>/tests
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()
set(EC_TEST_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# -----------------------------------------------------------------------------
# Compiler options shared by all Energy Crisis targets (not applied to SFML itself)
# -----------------------------------------------------------------------------
add_library(ec_options INTERFACE)
if(MSVC)
    # Sources are UTF-8 (Bulgarian text); /utf-8 keeps the strings intact
    target_compile_options(ec_options INTERFACE /utf-8 /W3 /permissive- /Zc:__cplusplus)
    target_compile_definitions(ec_options INTERFACE _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    target_compile_options(ec_options INTERFACE -Wall -Wextra)
endif()

# -----------------------------------------------------------------------------
# SFML 3 (only for the game; the engine needs just sf::Vector2f / sf::FloatRect)
# -----------------------------------------------------------------------------
set(EC_SFML_COMPONENTS Graphics Window System Audio)
if(ANDROID)
    list(APPEND EC_SFML_COMPONENTS Main)
endif()

function(ec_fetch_sfml)
    include(FetchContent)
    # Variables set here only reach the SFML sub-project, not the rest of this file
    set(BUILD_SHARED_LIBS OFF)
    set(SFML_BUILD_NETWORK OFF)
    set(SFML_BUILD_EXAMPLES OFF)
    set(SFML_BUILD_DOC OFF)
    set(SFML_BUILD_TEST_SUITE OFF)
    set(_ec_fetch_extra)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.25)
        list(APPEND _ec_fetch_extra SYSTEM)          # no warnings from SFML headers
    endif()
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.28)
        list(APPEND _ec_fetch_extra EXCLUDE_FROM_ALL) # do not install SFML with the game
    endif()
    FetchContent_Declare(SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG        ${EC_SFML_VERSION}
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
        ${_ec_fetch_extra})
    FetchContent_MakeAvailable(SFML)
endfunction()

set(EC_SFML_SHARED OFF)
if(EC_BUILD_GAME)
    if(EC_FETCH_SFML)
        find_package(SFML 3 QUIET COMPONENTS ${EC_SFML_COMPONENTS})
    else()
        find_package(SFML 3 COMPONENTS ${EC_SFML_COMPONENTS})
    endif()

    if(SFML_FOUND)
        message(STATUS "Energy Crisis: using installed SFML ${SFML_VERSION} (${SFML_DIR})")
    elseif(EC_FETCH_SFML)
        message(STATUS "Energy Crisis: no installed SFML 3 found, fetching SFML ${EC_SFML_VERSION} (static)")
        ec_fetch_sfml()
    else()
        message(FATAL_ERROR
            "SFML 3 was not found (SFML 2.x cannot build this game).\n"
            "  - point CMake to it:  -DSFML_DIR=<SFML>/lib/cmake/SFML  (or -DCMAKE_PREFIX_PATH=<SFML>)\n"
            "  - or let CMake download it:  -DEC_FETCH_SFML=ON\n"
            "  - or build only the engine and tests:  -DEC_BUILD_GAME=OFF")
    endif()

    get_target_property(_ec_sfml_type SFML::System TYPE)
    if(_ec_sfml_type STREQUAL "SHARED_LIBRARY")
        set(EC_SFML_SHARED ON)
    endif()

    find_package(SQLite3 REQUIRED)
endif()

# MinGW: the C++ runtime DLLs of the compiler, copied next to the programs so they start by double-click
set(EC_MINGW_RUNTIME_DLLS)
if(MINGW)
    get_filename_component(_ec_mingw_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(_dll libstdc++-6.dll libgcc_s_seh-1.dll libgcc_s_dw2-1.dll libwinpthread-1.dll)
        if(EXISTS "${_ec_mingw_bin}/${_dll}")
            list(APPEND EC_MINGW_RUNTIME_DLLS "${_ec_mingw_bin}/${_dll}")
        endif()
    endforeach()
endif()

# Copies the shared SFML and MinGW runtime DLLs a program needs into its output folder (Windows only)
function(ec_copy_runtime_dlls target)
    if(NOT WIN32)
        return()
    endif()
    if(EC_SFML_SHARED)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:${target}> $<TARGET_FILE_DIR:${target}>
            COMMAND_EXPAND_LISTS
            VERBATIM)
    endif()
    if(EC_MINGW_RUNTIME_DLLS)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EC_MINGW_RUNTIME_DLLS} $<TARGET_FILE_DIR:${target}>
            VERBATIM)
    endif()
endfunction()

# -----------------------------------------------------------------------------
# ec_engine: game rules, economy, weather, time (Game/)
# -----------------------------------------------------------------------------
file(GLOB EC_ENGINE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Game/scr/*.cpp")
file(GLOB EC_ENGINE_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Game/includes/*.h")

add_library(ec_engine STATIC ${EC_ENGINE_SOURCES} ${EC_ENGINE_HEADERS})
target_include_directories(ec_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Game/includes")
target_link_libraries(ec_engine PRIVATE ec_options)
if(EC_BUILD_GAME)
    # The engine includes <SFML/Graphics.hpp> for sf::Vector2f and sf::FloatRect
    target_link_libraries(ec_engine PUBLIC SFML::Graphics)
else()
    # Headless build: the same minimal stand-in for SFML that "make test" uses
    target_include_directories(ec_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/scratch/sfml_stub")
endif()

# -----------------------------------------------------------------------------
# energy_crisis: the game (main.cpp + UI/)
# -----------------------------------------------------------------------------
if(EC_BUILD_GAME)
    file(GLOB EC_UI_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/UI/scr/*.cpp")
    file(GLOB EC_UI_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/UI/includes/*.h")

    if(ANDROID)
        add_library(energy_crisis SHARED "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp" ${EC_UI_SOURCES} ${EC_UI_HEADERS})
        target_include_directories(energy_crisis PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/UI/includes")
        target_link_libraries(energy_crisis PRIVATE ec_engine ec_options
            SFML::Graphics SFML::Window SFML::System SFML::Audio
            android log OpenSLES)
        target_link_libraries(energy_crisis PUBLIC
            -Wl,--whole-archive
            SFML::Main
            -Wl,--no-whole-archive
        )
        target_link_options(energy_crisis PRIVATE "-Wl,-z,max-page-size=16384")
    else()
        add_executable(energy_crisis "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp" ${EC_UI_SOURCES} ${EC_UI_HEADERS})
        target_include_directories(energy_crisis PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/UI/includes")
        target_link_libraries(energy_crisis PRIVATE ec_engine ec_options
            SFML::Graphics SFML::Window SFML::System SFML::Audio SQLite::SQLite3)
        ec_copy_runtime_dlls(energy_crisis)

        # assets/ next to the executable. Re-copied (old files removed) whenever an asset is added,
        # changed or deleted; the asset list file changes when CONFIGURE_DEPENDS sees a new/removed file.
        file(GLOB_RECURSE EC_ASSET_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/*")
        set(EC_ASSET_LIST "${CMAKE_CURRENT_BINARY_DIR}/ec_assets.list")
        string(REPLACE ";" "\n" _ec_asset_lines "${EC_ASSET_FILES}")
        file(CONFIGURE OUTPUT "${EC_ASSET_LIST}" CONTENT "${_ec_asset_lines}\n" @ONLY)
        set(EC_ASSET_STAMP "${CMAKE_CURRENT_BINARY_DIR}/ec_assets_$<CONFIG>.stamp")
        add_custom_command(
            OUTPUT "${EC_ASSET_STAMP}"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:energy_crisis>/assets"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:energy_crisis>/assets"
            COMMAND ${CMAKE_COMMAND} -E touch "${EC_ASSET_STAMP}"
            DEPENDS ${EC_ASSET_FILES} "${EC_ASSET_LIST}"
            COMMENT "Copying assets/ next to energy_crisis"
            VERBATIM)
        add_custom_target(ec_assets ALL DEPENDS "${EC_ASSET_STAMP}")
        add_dependencies(energy_crisis ec_assets)

        add_custom_target(run
            COMMAND energy_crisis
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:energy_crisis>"
            USES_TERMINAL
            COMMENT "Starting Energy Crisis")

        # cmake --install build --prefix dist  ->  a folder that can be zipped and played
        install(TARGETS energy_crisis RUNTIME DESTINATION .)
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION .)
        if(WIN32 AND EC_SFML_SHARED)
            install(FILES $<TARGET_RUNTIME_DLLS:energy_crisis> DESTINATION .)
        endif()
        if(EC_MINGW_RUNTIME_DLLS)
            install(FILES ${EC_MINGW_RUNTIME_DLLS} DESTINATION .)
        endif()

        set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT energy_crisis)
        set_target_properties(energy_crisis PROPERTIES
            VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:energy_crisis>")
    endif()
endif()

# -----------------------------------------------------------------------------
# Headless engine tests: one CTest test per scratch/test_*.cpp
#   ctest --test-dir build --output-on-failure
#   EC_SEED=42 ctest --test-dir build      (same weather on every run)
# -----------------------------------------------------------------------------
if(EC_BUILD_TESTS AND NOT ANDROID)
    enable_testing()
    file(GLOB EC_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scratch/test_*.cpp")
    list(SORT EC_TEST_SOURCES)
    foreach(_src IN LISTS EC_TEST_SOURCES)
        get_filename_component(_name "${_src}" NAME_WE)
        add_executable(${_name} "${_src}")
        target_link_libraries(${_name} PRIVATE ec_engine ec_options)
        set_target_properties(${_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${EC_TEST_OUTPUT_DIRECTORY}")
        ec_copy_runtime_dlls(${_name})
        add_test(NAME ${_name} COMMAND ${_name})
        set_tests_properties(${_name} PROPERTIES TIMEOUT ${EC_TEST_TIMEOUT} LABELS engine WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    endforeach()
    list(LENGTH EC_TEST_SOURCES _ec_test_count)
    message(STATUS "Energy Crisis: ${_ec_test_count} engine tests registered with CTest")
endif()

message(STATUS "Energy Crisis: game=${EC_BUILD_GAME} tests=${EC_BUILD_TESTS} fetch_sfml=${EC_FETCH_SFML} build_type=${CMAKE_BUILD_TYPE}")
