cmake_minimum_required(VERSION 3.28)
include(evogine/src/evogine.cmake)
include(evogine/src/cmake.cmake)
set(APPLICATION_TARGET evo-tournament)
project(evo-tournament LANGUAGES C CXX)

set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})
file(GLOB_RECURSE PROJECT_SOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")
file(GLOB_RECURSE PROJECT_HEADER_FILES "${CMAKE_CURRENT_LIST_DIR}/src/*.h")
add_executable(evo-tournament ${PROJECT_SOURCE_FILES} ${PROJECT_HEADER_FILES})
target_include_directories(evo-tournament PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src/")

set(WINDOW_ENABLED ON CACHE BOOL "Enable feature" FORCE)
set(WINDOW_BACKEND sdl CACHE STRING "Set backend" FORCE)
set(INPUT_ENABLED ON CACHE BOOL "Enable feature" FORCE)
set(INPUT_BACKEND sdl CACHE STRING "Set backend" FORCE)
set(ECS_ENABLED ON CACHE BOOL "Enable feature" FORCE)
set(GRAPHICS_ENABLED ON CACHE BOOL "Enable feature" FORCE)
set(GRAPHICS_BACKEND opengl CACHE STRING "Set backend" FORCE)
set(RESOURCE_ENABLED ON CACHE BOOL "Enable feature" FORCE)
set(NLOHMANNJSON_EXTENSION_ENABLED ON CACHE BOOL "Enable feature" FORCE)
set(STB_EXTENSION_ENABLED ON CACHE BOOL "Enable feature" FORCE)
set(SDL_EXTENSION_ENABLED ON CACHE BOOL "Enable feature" FORCE)
set(UTILS_ENABLED ON CACHE BOOL "Enable feature" FORCE)
set(AUDIO_ENABLED ON CACHE BOOL "Enable feature" FORCE)
set(AUDIO_BACKEND miniaudio CACHE STRING "Set backend" FORCE)
# Menus and the HUD are RmlUi documents (resources/ui).
set(IMGUI_ENABLED OFF CACHE BOOL "Enable feature" FORCE)
set(IMGUI_EXTENSION_ENABLED OFF CACHE BOOL "Enable feature" FORCE)
set(UI_ENABLED ON CACHE BOOL "Enable feature" FORCE)
set(UI_BACKEND rmlui CACHE STRING "Set backend" FORCE)
set(OS_ENABLED ON CACHE BOOL "Enable feature" FORCE)
set(NET_ENABLED ON CACHE BOOL "Enable feature" FORCE)
if(EMSCRIPTEN)
    set(NET_BACKEND webrtc CACHE STRING "Set backend" FORCE)
else()
    set(NET_BACKEND udp CACHE STRING "Set backend" FORCE)
endif()

# Local fixes to the pinned engine revision (see patches/README). Applied once, idempotently.
find_package(Git QUIET)
file(GLOB EVO_ENGINE_PATCHES "${CMAKE_CURRENT_LIST_DIR}/patches/*.patch")
foreach(engine_patch IN LISTS EVO_ENGINE_PATCHES)
    execute_process(
            COMMAND ${GIT_EXECUTABLE} apply --reverse --check "${engine_patch}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/evogine"
            RESULT_VARIABLE already_applied OUTPUT_QUIET ERROR_QUIET)
    if(NOT already_applied EQUAL 0)
        execute_process(
                COMMAND ${GIT_EXECUTABLE} apply "${engine_patch}"
                WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/evogine"
                RESULT_VARIABLE apply_result)
        if(NOT apply_result EQUAL 0)
            message(FATAL_ERROR "Failed to apply engine patch ${engine_patch}")
        endif()
        message(STATUS "Applied engine patch ${engine_patch}")
    endif()
endforeach()

add_subdirectory(evogine/src engine_build)

target_link_libraries(evo-tournament PRIVATE evogine)
if(EMSCRIPTEN)
    target_compile_definitions(evo-tournament PRIVATE EVO_RESOURCE_DIR="/game")
else()
    target_compile_definitions(evo-tournament PRIVATE EVO_RESOURCE_DIR="${CMAKE_CURRENT_LIST_DIR}/resources")
endif()
ADD_RESOURCE_PATH("game,${CMAKE_CURRENT_LIST_DIR}/resources")
GENERATE_RESOURCE_HEADER()
generate_components()
GENERATE_ECS_HEADER()

if(EMSCRIPTEN)
    foreach(resource_path_pair IN LISTS PHYSICAL_MOUNT_PATH_LIST)
        string(REPLACE "," ";" parts "${resource_path_pair}")
        list(GET parts 0 mount_name)
        list(GET parts 1 mount_path)
        target_link_options(evo-tournament PRIVATE
                "SHELL:--preload-file \"${mount_path}@/${mount_name}\""
        )
    endforeach()
    set_target_properties(evo-tournament PROPERTIES SUFFIX ".html")
    target_link_options(evo-tournament PRIVATE
            "-sMIN_WEBGL_VERSION=2"
            "-sMAX_WEBGL_VERSION=2"
            "-sALLOW_MEMORY_GROWTH=1"
            "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','stringToUTF8','HEAPU8','HEAPF32','HEAP32']"
            "SHELL:--shell-file ${CMAKE_CURRENT_LIST_DIR}/web/shell.html"
    )
    set_property(TARGET evo-tournament APPEND PROPERTY LINK_DEPENDS
            "${CMAKE_CURRENT_LIST_DIR}/web/shell.html"
    )
endif()
