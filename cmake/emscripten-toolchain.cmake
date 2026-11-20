if(DEFINED ENV{EMSDK})
    set(EVO_EMSDK_ROOT "$ENV{EMSDK}")
else()
    set(EVO_EMSDK_ROOT "$ENV{HOME}/emsdk")
endif()

set(EVO_EMSCRIPTEN_PLATFORM "${EVO_EMSDK_ROOT}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
if(NOT EXISTS "${EVO_EMSCRIPTEN_PLATFORM}")
    message(FATAL_ERROR "Emscripten not found at ${EVO_EMSDK_ROOT}; set EMSDK")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
include("${EVO_EMSCRIPTEN_PLATFORM}")

if(EMSCRIPTEN)
    add_link_options("-sMIN_WEBGL_VERSION=2" "-sMAX_WEBGL_VERSION=2")
endif()

get_filename_component(EVOGINE_PLATFORM_PATH
        "${CMAKE_CURRENT_LIST_DIR}/../evogine/src/platform/emscripten" ABSOLUTE)
set(EVOGINE_PLATFORM_PATH "${EVOGINE_PLATFORM_PATH}" CACHE PATH "" FORCE)

set(OS_HAS_FILESYSTEM ON CACHE STRING "Does OS have a filesystem" FORCE)
set(OS_HAS_COMMANDLINE ON CACHE STRING "Does OS support commandline" FORCE)

# Some engine headers rely on libstdc++'s transitive includes; libc++ is stricter.
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -include cmath -include algorithm -include cstring -include cstdlib -include vector -include string -include functional -include memory")
