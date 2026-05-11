cmake_minimum_required(VERSION 3.10.0)
project(LavenderHook)


set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_SOURCE_DIR}/include)

link_directories(${CMAKE_SOURCE_DIR}/libs)

# Source files
file(GLOB_RECURSE SRC_FILES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.c
)

add_library(LavenderHook SHARED ${SRC_FILES})

target_sources(LavenderHook PRIVATE)

target_compile_definitions(LavenderHook PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

set(RESOURCE_FILES
    src/assets/resources/resources.rc
)

target_sources(LavenderHook PRIVATE ${RESOURCE_FILES})

# Link dependencies
target_link_libraries(LavenderHook
    d3dx11.lib
    Ws2_32
    Iphlpapi
    Shell32
    Ole32
    User32
    Gdi32
    Dwmapi
    Version
    Xinput
    ViGEmClient
    Setupapi
    Winhttp
)

if(MSVC)
    target_compile_options(LavenderHook PRIVATE /MT)
endif()

# Export settings
set_target_properties(LavenderHook PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)
  
