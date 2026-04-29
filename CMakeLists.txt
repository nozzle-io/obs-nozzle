cmake_minimum_required(VERSION 3.20)

project(obs-nozzle
    VERSION 0.1.0
    DESCRIPTION "Nozzle GPU texture sharing plugin for OBS Studio"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- nozzle submodule ---
set(NOZZLE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NOZZLE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(nozzle)

# --- Find OBS Studio ---
if(NOT DEFINED OBS_INCLUDE_DIR)
    if(DEFINED ENV{OBS_STUDIO_DIR})
        set(OBS_INCLUDE_DIR "$ENV{OBS_STUDIO_DIR}/libobs" CACHE PATH "OBS Studio include directory")
    else()
        find_path(OBS_INCLUDE_DIR
            NAMES obs.h obs-module.h
            PATHS
                /usr/include/obs
                /usr/local/include/obs
                /opt/homebrew/include/obs
                "$ENV{HOME}/obs-studio/libobs"
        )
    endif()
endif()

if(NOT OBS_INCLUDE_DIR OR NOT EXISTS "${OBS_INCLUDE_DIR}/obs.h")
    message(FATAL_ERROR
        "OBS Studio headers not found. Set -DOBS_INCLUDE_DIR=<path> or OBS_INCLUDE_DIR env var.\n"
        "  On macOS with homebrew: /opt/homebrew/include/obs\n"
        "  On Linux: /usr/include/obs\n"
        "  Build from source: point to <obs-studio-source>/libobs"
    )
endif()

# --- OBS library ---
option(NOZZLE_OBS_LINK_LIBRARY "Link against OBS library (disable for header-only CI build)" ON)

if(NOZZLE_OBS_LINK_LIBRARY)
    if(NOT DEFINED OBS_LIBRARY)
        if(APPLE)
            find_library(OBS_LIBRARY
                NAMES obs.0 obs
                PATHS
                    /opt/homebrew/lib
                    /usr/local/lib
                    "$ENV{HOME}/obs-studio/build/libobs"
            )
        elseif(UNIX)
            find_library(OBS_LIBRARY
                NAMES obs.0 obs
                PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib
            )
        elseif(WIN32)
            find_library(OBS_LIBRARY
                NAMES obs obs64
                PATHS
                    "$ENV{OBS_STUDIO_DIR}"
                    "$ENV{OBS_STUDIO_DIR}/libobs"
                    "$ENV{PROGRAMFILES}/obs-studio/libobs"
            )
        endif()
    endif()

    if(NOT OBS_LIBRARY)
        message(FATAL_ERROR
            "OBS Studio library not found. Set -DOBS_LIBRARY=<path-to-libobs>.\n"
            "  On macOS: typically in /opt/homebrew/lib/libobs.0.dylib\n"
            "  On Linux: typically in /usr/lib/x86_64-linux-gnu/libobs.so.0"
        )
    endif()
endif()

# --- Plugin shared library ---
add_library(${PROJECT_NAME} MODULE)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${OBS_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    $<$<AND:<BOOL:${OBS_DEPS_INCLUDE_DIR}>,<EXISTS:${OBS_DEPS_INCLUDE_DIR}>>:${OBS_DEPS_INCLUDE_DIR}>
)

target_sources(${PROJECT_NAME} PRIVATE
    src/nozzle-plugin.cpp
    src/nozzle-source.cpp
    src/nozzle-output.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    nozzle
    $<$<BOOL:${NOZZLE_OBS_LINK_LIBRARY}>:${OBS_LIBRARY}>
)

# Disable exceptions (OBS plugin is C API, nozzle is no-exceptions)
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:-fno-exceptions -fno-rtti>
    $<$<CXX_COMPILER_ID:GNU>:-fno-exceptions -fno-rtti>
    $<$<CXX_COMPILER_ID:MSVC>:/EHs-c- /GR->
)

# Platform-specific link
if(APPLE)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${COREFOUNDATION_FRAMEWORK})
    # macOS deployment target
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "" FORCE)
    endif()
    # .obsplugin bundle
    set_target_properties(${PROJECT_NAME} PROPERTIES
        PREFIX ""
        SUFFIX ".so"
    )
elseif(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        PREFIX ""
        SUFFIX ".dll"
    )
elseif(UNIX)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        PREFIX ""
        SUFFIX ".so"
    )
endif()

# --- Data files (locale) ---
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/data
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/../data
    COMMENT "Copying plugin data files"
)

# --- Install ---
include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/obs-plugins/${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/obs-plugins/${PROJECT_NAME}
)

install(DIRECTORY data/
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/obs-plugins/${PROJECT_NAME}
)
