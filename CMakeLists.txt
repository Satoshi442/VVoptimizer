cmake_minimum_required(VERSION 3.22)

project(vv_optimizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT ANDROID)
    message(FATAL_ERROR "This project builds the Android native mod only.")
endif()

include(FetchContent)

FetchContent_Declare(
    preloader_android
    GIT_REPOSITORY https://github.com/LiteLDev/preloader-android.git
    GIT_TAG 0.2.2
)

FetchContent_MakeAvailable(preloader_android)

add_library(vv_optimizer SHARED
    src/VVOptimizer.cpp
)

target_link_libraries(vv_optimizer PRIVATE
    preloader
    android
    log
    dl
)

set_target_properties(vv_optimizer PROPERTIES
    OUTPUT_NAME "vv_optimizer"
)
