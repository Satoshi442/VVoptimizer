cmake_minimum_required(VERSION 3.22)
project(vv_optimizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PRELOADER_ANDROID_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/preloader-android"
    CACHE PATH "Path to preloader-android")

if(NOT EXISTS "${PRELOADER_ANDROID_ROOT}/include/pl/Mod.hpp")
    message(FATAL_ERROR
        "PRELOADER_ANDROID_ROOT must point to preloader-android. "
        "Missing: ${PRELOADER_ANDROID_ROOT}/include/pl/Mod.hpp")
endif()

if(NOT ANDROID)
    message(FATAL_ERROR "This project builds the Android native mod only.")
endif()

if(NOT TARGET preloader)
    add_subdirectory("${PRELOADER_ANDROID_ROOT}" "${CMAKE_BINARY_DIR}/_preloader_android")
endif()

add_library(vv_optimizer SHARED
    src/VVOptimizer.cpp
)

target_include_directories(vv_optimizer PRIVATE
    "${PRELOADER_ANDROID_ROOT}/include"
)

target_compile_options(vv_optimizer PRIVATE
    -Wall -Wextra -Wpedantic
    -fvisibility=hidden
    -fvisibility-inlines-hidden
)

target_link_libraries(vv_optimizer PRIVATE
    preloader
    android
    log
    dl
)

set_target_properties(vv_optimizer PROPERTIES
    OUTPUT_NAME "vv_optimizer"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
