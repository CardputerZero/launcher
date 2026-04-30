cmake_minimum_required(VERSION 3.16)
project(cardputer-zero-emu LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Paths — same repo, sibling projects
set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(APPLAUNCH  ${REPO_ROOT}/projects/APPLaunch/main)
set(USERDEMO   ${REPO_ROOT}/projects/UserDemo/main)

# ---------------------------------------------------------------------------
# SDL2 + Freetype (skip for Emscripten — uses its own ports)
# ---------------------------------------------------------------------------
if(NOT EMSCRIPTEN)
    if(APPLE)
        if(EXISTS "/opt/homebrew/opt/sdl2")
            list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/sdl2")
        elseif(EXISTS "/usr/local/opt/sdl2")
            list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/sdl2")
        endif()
    endif()

    find_package(SDL2 QUIET)
    if(NOT SDL2_FOUND)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SDL2 REQUIRED sdl2)
    endif()

    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(FREETYPE freetype2)
        pkg_check_modules(SDL2_MIXER SDL2_mixer)
    endif()
    if(NOT FREETYPE_FOUND)
        find_package(Freetype QUIET)
        if(Freetype_FOUND)
            set(FREETYPE_INCLUDE_DIRS ${FREETYPE_INCLUDE_DIRS})
            set(FREETYPE_LIBRARIES Freetype::Freetype)
        endif()
    endif()
endif()

# ---------------------------------------------------------------------------
# LVGL 9.5
# ---------------------------------------------------------------------------
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_THORVG OFF CACHE BOOL "" FORCE)
set(LV_BUILD_CONF_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "" FORCE)
add_definitions(-DLV_CONF_INCLUDE_SIMPLE)
if(EMSCRIPTEN OR WIN32)
    add_definitions(-DLV_USE_FREETYPE=0)
endif()
if(EMSCRIPTEN)
    add_compile_options(-sUSE_SDL=2 -pthread)
endif()

# SDL2 parent include for <SDL2/SDL.h>
if(NOT EMSCRIPTEN)
    if(SDL2_FOUND AND TARGET SDL2::SDL2)
        get_target_property(_sdl2_inc SDL2::SDL2 INTERFACE_INCLUDE_DIRECTORIES)
        if(_sdl2_inc)
            foreach(_dir ${_sdl2_inc})
                get_filename_component(_parent "${_dir}" DIRECTORY)
                include_directories("${_parent}")
            endforeach()
        endif()
    else()
        foreach(_dir ${SDL2_INCLUDE_DIRS})
            get_filename_component(_parent "${_dir}" DIRECTORY)
            include_directories("${_parent}")
        endforeach()
    endif()
    if(FREETYPE_INCLUDE_DIRS)
        include_directories(${FREETYPE_INCLUDE_DIRS})
    endif()
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib/lvgl ${CMAKE_CURRENT_BINARY_DIR}/lvgl)

# ---------------------------------------------------------------------------
# Helper: common link libraries for app targets
# ---------------------------------------------------------------------------
macro(emu_link_deps target)
    target_link_libraries(${target} PRIVATE lvgl)
    if(TARGET SDL2::SDL2)
        target_link_libraries(${target} PRIVATE SDL2::SDL2)
    else()
        target_link_libraries(${target} PRIVATE ${SDL2_LIBRARIES})
    endif()
    if(FREETYPE_FOUND AND FREETYPE_LINK_LIBRARIES)
        target_link_libraries(${target} PRIVATE ${FREETYPE_LINK_LIBRARIES})
        target_link_directories(${target} PRIVATE ${FREETYPE_LIBRARY_DIRS})
    elseif(FREETYPE_LIBRARIES)
        target_link_libraries(${target} PRIVATE ${FREETYPE_LIBRARIES})
    else()
        target_link_directories(${target} PRIVATE /opt/homebrew/lib)
        target_link_libraries(${target} PRIVATE freetype)
    endif()
    find_package(Threads REQUIRED)
    target_link_libraries(${target} PRIVATE Threads::Threads)
endmacro()

# ===========================================================================
# Native-only targets (skipped for Emscripten)
# ===========================================================================
if(NOT EMSCRIPTEN)

option(EMU_SKIP_APPLAUNCH "Skip APPLaunch (Unix-only deps)" OFF)

file(GLOB_RECURSE APPLAUNCH_UI_C   ${APPLAUNCH}/ui/*.c)
file(GLOB_RECURSE APPLAUNCH_UI_CPP ${APPLAUNCH}/ui/*.cpp)
file(GLOB         APPLAUNCH_KBD    ${APPLAUNCH}/src/keyboard_input.c)
file(GLOB APPLAUNCH_HAL_SDL_C   ${APPLAUNCH}/hal/sdl/*.c)
file(GLOB APPLAUNCH_HAL_SDL_CPP ${APPLAUNCH}/hal/sdl/*.cpp)

# ---------------------------------------------------------------------------
# APP: APPLaunch shared lib (Unix only)
# ---------------------------------------------------------------------------
if(NOT EMU_SKIP_APPLAUNCH AND NOT WIN32)

add_library(APPLaunch SHARED
    ${APPLAUNCH_UI_C}
    ${APPLAUNCH_UI_CPP}
    ${APPLAUNCH_KBD}
    ${APPLAUNCH_HAL_SDL_C}
    ${APPLAUNCH_HAL_SDL_CPP}
)
target_include_directories(APPLaunch PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${APPLAUNCH}/ui
    ${APPLAUNCH}/include
    ${APPLAUNCH}
    ${APPLAUNCH}/hal
)
target_compile_definitions(APPLaunch PRIVATE HAL_PLATFORM_SDL=1)
if(SDL2_MIXER_FOUND)
    target_compile_definitions(APPLaunch PRIVATE EMU_HAS_AUDIO=1)
    target_include_directories(APPLaunch PRIVATE ${SDL2_MIXER_INCLUDE_DIRS})
    target_link_libraries(APPLaunch PRIVATE ${SDL2_MIXER_LINK_LIBRARIES})
    target_link_directories(APPLaunch PRIVATE ${SDL2_MIXER_LIBRARY_DIRS})
endif()
if(WIN32)
    target_include_directories(APPLaunch BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/compat)
    target_compile_options(APPLaunch PRIVATE
        "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/src/emu_compat_win.h"
        -Wno-c++11-narrowing)
else()
    target_compile_options(APPLaunch PRIVATE
        "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/src/emu_compat.h"
        -Wno-c++11-narrowing)
endif()
if(APPLE)
    target_link_options(APPLaunch PRIVATE -undefined dynamic_lookup)
else()
    target_link_options(APPLaunch PRIVATE -Wl,--unresolved-symbols=ignore-all)
endif()
endif() # NOT EMU_SKIP_APPLAUNCH

# ---------------------------------------------------------------------------
# APP: UserDemo shared lib
# ---------------------------------------------------------------------------
file(GLOB_RECURSE USERDEMO_UI_C ${USERDEMO}/ui/*.c)

add_library(UserDemo SHARED ${USERDEMO_UI_C})
target_include_directories(UserDemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${USERDEMO}/ui
    ${USERDEMO}/include
    ${USERDEMO}
)
target_compile_options(UserDemo PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/src/emu_compat.h)
if(APPLE)
    target_link_options(UserDemo PRIVATE -undefined dynamic_lookup)
elseif(WIN32)
    target_link_libraries(UserDemo PRIVATE lvgl)
else()
    target_link_options(UserDemo PRIVATE -Wl,--unresolved-symbols=ignore-all)
endif()

# ---------------------------------------------------------------------------
# Emulator executable
# ---------------------------------------------------------------------------
if(WIN32 AND NOT EMU_SKIP_APPLAUNCH)
    add_executable(cardputer-zero-emu src/main.cpp src/device_skin.cpp src/keyboard_map.cpp
        ${APPLAUNCH_UI_C} ${APPLAUNCH_UI_CPP} ${APPLAUNCH_KBD}
        ${APPLAUNCH_HAL_SDL_C} ${APPLAUNCH_HAL_SDL_CPP})
    target_include_directories(cardputer-zero-emu PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/compat
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor
        ${APPLAUNCH}/ui ${APPLAUNCH}/include ${APPLAUNCH} ${APPLAUNCH}/hal)
    target_compile_options(cardputer-zero-emu PRIVATE
        "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/src/emu_compat_win.h"
        -Wno-c++11-narrowing)
    target_compile_definitions(cardputer-zero-emu PRIVATE EMU_STATIC_APP=1 HAL_PLATFORM_SDL=1)
    target_link_options(cardputer-zero-emu PRIVATE -Wl,--allow-multiple-definition)
elseif(WIN32)
    add_executable(cardputer-zero-emu src/main.cpp src/device_skin.cpp src/keyboard_map.cpp ${USERDEMO_UI_C})
    target_include_directories(cardputer-zero-emu PRIVATE
        ${USERDEMO}/ui ${USERDEMO}/include ${USERDEMO})
    target_compile_definitions(cardputer-zero-emu PRIVATE EMU_STATIC_APP=1)
else()
    add_executable(cardputer-zero-emu src/main.cpp src/device_skin.cpp src/keyboard_map.cpp)
endif()
add_dependencies(cardputer-zero-emu lvgl lvgl_thorvg)
if(APPLE)
    set_target_properties(cardputer-zero-emu PROPERTIES ENABLE_EXPORTS ON)
endif()

target_include_directories(cardputer-zero-emu PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)
if(NOT MSVC)
    target_compile_options(cardputer-zero-emu PRIVATE -Wno-c++11-narrowing)
endif()

# Force-load LVGL for dlopen'd apps
if(APPLE)
    target_link_libraries(cardputer-zero-emu PRIVATE
        -Wl,-force_load,$<TARGET_FILE:lvgl>
        -Wl,-force_load,$<TARGET_FILE:lvgl_thorvg>
    )
elseif(MSVC)
    target_link_libraries(cardputer-zero-emu PRIVATE lvgl lvgl_thorvg)
    target_link_options(cardputer-zero-emu PRIVATE
        /WHOLEARCHIVE:lvgl /WHOLEARCHIVE:lvgl_thorvg)
elseif(WIN32)
    target_link_libraries(cardputer-zero-emu PRIVATE lvgl lvgl_thorvg)
else()
    target_link_libraries(cardputer-zero-emu PRIVATE
        -Wl,--whole-archive $<TARGET_FILE:lvgl> $<TARGET_FILE:lvgl_thorvg> -Wl,--no-whole-archive
    )
endif()

# SDL2
if(TARGET SDL2::SDL2)
    target_link_libraries(cardputer-zero-emu PRIVATE SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
    target_link_libraries(cardputer-zero-emu PRIVATE SDL2::SDL2-static)
else()
    target_include_directories(cardputer-zero-emu PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(cardputer-zero-emu PRIVATE ${SDL2_LIBRARIES})
endif()
if(TARGET SDL2::SDL2main)
    target_link_libraries(cardputer-zero-emu PRIVATE SDL2::SDL2main)
endif()

# SDL2_image
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2_IMAGE SDL2_image)
endif()
if(SDL2_IMAGE_FOUND)
    target_include_directories(cardputer-zero-emu PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
    target_link_libraries(cardputer-zero-emu PRIVATE ${SDL2_IMAGE_LINK_LIBRARIES})
    target_link_directories(cardputer-zero-emu PRIVATE ${SDL2_IMAGE_LIBRARY_DIRS})
else()
    target_link_directories(cardputer-zero-emu PRIVATE /opt/homebrew/lib)
    target_link_libraries(cardputer-zero-emu PRIVATE SDL2_image)
endif()

# Freetype
if(FREETYPE_FOUND AND FREETYPE_LINK_LIBRARIES)
    target_link_libraries(cardputer-zero-emu PRIVATE ${FREETYPE_LINK_LIBRARIES})
    target_link_directories(cardputer-zero-emu PRIVATE ${FREETYPE_LIBRARY_DIRS})
else()
    target_link_directories(cardputer-zero-emu PRIVATE /opt/homebrew/lib)
    target_link_libraries(cardputer-zero-emu PRIVATE freetype)
endif()

# SDL2_mixer (optional)
if(SDL2_MIXER_FOUND)
    target_include_directories(cardputer-zero-emu PRIVATE ${SDL2_MIXER_INCLUDE_DIRS})
    target_link_libraries(cardputer-zero-emu PRIVATE ${SDL2_MIXER_LINK_LIBRARIES})
    target_link_directories(cardputer-zero-emu PRIVATE ${SDL2_MIXER_LIBRARY_DIRS})
    target_compile_definitions(cardputer-zero-emu PRIVATE EMU_HAS_AUDIO=1)
else()
    find_library(SDL2_MIXER_LIB SDL2_mixer)
    if(SDL2_MIXER_LIB)
        target_link_libraries(cardputer-zero-emu PRIVATE ${SDL2_MIXER_LIB})
        target_compile_definitions(cardputer-zero-emu PRIVATE EMU_HAS_AUDIO=1)
    endif()
endif()

# dl + threads
if(NOT WIN32)
    target_link_libraries(cardputer-zero-emu PRIVATE ${CMAKE_DL_LIBS})
    find_package(Threads REQUIRED)
    target_link_libraries(cardputer-zero-emu PRIVATE Threads::Threads)
else()
    # ui_app_hack.hpp calls gethostname() on Windows via <winsock2.h>.
    target_link_libraries(cardputer-zero-emu PRIVATE ws2_32)
endif()

# ---------------------------------------------------------------------------
# Post-build: copy apps and resources
# ---------------------------------------------------------------------------
add_custom_command(TARGET cardputer-zero-emu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:cardputer-zero-emu>/apps
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:UserDemo> $<TARGET_FILE_DIR:cardputer-zero-emu>/apps/
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:cardputer-zero-emu>/applications
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:cardputer-zero-emu>/images
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${APPLAUNCH}/ui/images $<TARGET_FILE_DIR:cardputer-zero-emu>/images
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:cardputer-zero-emu>/assets
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/assets/device_skin.png $<TARGET_FILE_DIR:cardputer-zero-emu>/assets/device_skin.png
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/assets/startup.mp3 $<TARGET_FILE_DIR:cardputer-zero-emu>/assets/startup.mp3
    COMMENT "Copying apps and resources"
)
if(TARGET APPLaunch)
    add_custom_command(TARGET cardputer-zero-emu POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:APPLaunch> $<TARGET_FILE_DIR:cardputer-zero-emu>/apps/
        COMMENT "Copying APPLaunch"
    )
endif()

endif() # NOT EMSCRIPTEN

# ---------------------------------------------------------------------------
# Web (Emscripten) target
# ---------------------------------------------------------------------------
if(EMSCRIPTEN)
    file(GLOB_RECURSE APPLAUNCH_UI_C_WEB   ${APPLAUNCH}/ui/*.c)
    file(GLOB_RECURSE APPLAUNCH_UI_CPP_WEB ${APPLAUNCH}/ui/*.cpp)
    file(GLOB         APPLAUNCH_KBD_WEB    ${APPLAUNCH}/src/keyboard_input.c)
    file(GLOB         APPLAUNCH_HAL_SDL_C_WEB   ${APPLAUNCH}/hal/sdl/*.c)
    file(GLOB         APPLAUNCH_HAL_SDL_CPP_WEB ${APPLAUNCH}/hal/sdl/*.cpp)

    add_executable(cardputer-zero-emu-web
        src/main_web.cpp
        src/device_skin.cpp
        src/keyboard_map.cpp
        ${APPLAUNCH_UI_C_WEB}
        ${APPLAUNCH_UI_CPP_WEB}
        ${APPLAUNCH_KBD_WEB}
        ${APPLAUNCH_HAL_SDL_C_WEB}
        ${APPLAUNCH_HAL_SDL_CPP_WEB}
    )

    target_include_directories(cardputer-zero-emu-web PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/compat_queue
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        ${APPLAUNCH}/ui
        ${APPLAUNCH}/include
        ${APPLAUNCH}
        ${APPLAUNCH}/hal
    )

    target_compile_options(cardputer-zero-emu-web PRIVATE
        -include ${CMAKE_CURRENT_SOURCE_DIR}/src/emu_compat.h
        -Wno-c++11-narrowing
        -DLV_USE_FREETYPE=0
        -DHAL_PLATFORM_SDL=1
    )

    target_link_libraries(cardputer-zero-emu-web PRIVATE lvgl)

    target_compile_options(cardputer-zero-emu-web PRIVATE -sUSE_SDL=2 -sUSE_SDL_IMAGE=2 -pthread)
    target_link_options(cardputer-zero-emu-web PRIVATE
        -sUSE_SDL=2
        -sUSE_SDL_IMAGE=2
        -pthread -sPROXY_TO_PTHREAD=0
        -sSDL2_IMAGE_FORMATS=["png"]
        -sALLOW_MEMORY_GROWTH=1
        -sINITIAL_MEMORY=67108864
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/assets/device_skin.png@/assets/device_skin.png"
        "--preload-file=${APPLAUNCH}/ui/images@/images"
        --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html
    )

    set_target_properties(cardputer-zero-emu-web PROPERTIES
        SUFFIX ".html"
        OUTPUT_NAME "index"
    )
endif()
