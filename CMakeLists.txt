cmake_minimum_required(VERSION 3.24)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(ZenGinDevBridge
    VERSION 0.2.0
    DESCRIPTION "Local diagnostic bridge for Gothic II: Night of the Raven"
    LANGUAGES CXX
)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "ZenGin DevBridge currently builds with MSVC for Windows x86.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "ZenGin DevBridge targets the 32-bit Gothic II engine. Configure an x86 MSVC toolchain.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GENERATE_UNION_API_LIB ON CACHE BOOL "" FORCE)
set(GENERATE_UNION_API_DLL OFF CACHE BOOL "" FORCE)
set(GENERATE_UNION_API_EXE OFF CACHE BOOL "" FORCE)

set(GOTHIC_API_G1 OFF CACHE BOOL "" FORCE)
set(GOTHIC_API_G1A OFF CACHE BOOL "" FORCE)
set(GOTHIC_API_G2 OFF CACHE BOOL "" FORCE)
set(GOTHIC_API_G2A ON CACHE BOOL "" FORCE)
set(SPACER_API_S1 OFF CACHE BOOL "" FORCE)
set(SPACER_API_S2A OFF CACHE BOOL "" FORCE)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/union-api/CMakeLists.txt")
    message(FATAL_ERROR "Union API submodule is missing. Run: git submodule update --init --recursive")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/gothic-api/CMakeLists.txt")
    message(FATAL_ERROR "Gothic API submodule is missing. Run: git submodule update --init --recursive")
endif()

add_subdirectory(third_party/union-api)
add_subdirectory(third_party/gothic-api)

add_library(ZenGinDevBridge SHARED
    src/Plugin_g2a.cpp
)

target_include_directories(ZenGinDevBridge PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(ZenGinDevBridge PRIVATE
    DEVBRIDGE_VERSION="${PROJECT_VERSION}"
    DEVBRIDGE_PROTOCOL_VERSION=1
)

target_compile_options(ZenGinDevBridge PRIVATE
    /EHsc
    /W4
    /permissive-
    /Zc:__cplusplus
)

target_link_libraries(ZenGinDevBridge PRIVATE
    union_api_lib
    gothic_api
)

set_target_properties(ZenGinDevBridge PROPERTIES
    OUTPUT_NAME "ZenGinDevBridge"
    PREFIX ""
)

install(TARGETS ZenGinDevBridge
    RUNTIME DESTINATION System/Autorun
    LIBRARY DESTINATION System/Autorun
)
