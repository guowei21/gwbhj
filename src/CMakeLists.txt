cmake_minimum_required(VERSION 3.21)
project(GWBHJ)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Os -flto -ffunction-sections -fdata-sections)
add_link_options(-flto -Wl,--gc-sections)

if(NOT DEBUG)
    add_compile_options(-g0 -DNDEBUG)
    add_link_options(-s)
endif()

add_library(spoof SHARED
            spoof_module.cpp
            atexit.cpp
            sha256.cpp
            ed25519.cpp
            mini_json.cpp)

find_library(log-lib log)

target_include_directories(spoof PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(log-lib)
    target_link_libraries(spoof ${log-lib})
endif()

set_target_properties(spoof PROPERTIES
    LINK_FLAGS "-Wl,--exclude-libs,ALL"
)
