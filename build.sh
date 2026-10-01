#!/usr/bin/env bash
set -euo pipefail
: "${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME}"
PRELOADER_ROOT="${PRELOADER_ROOT:-$PWD/preloader-android}"
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24 \
  -DPRELOADER_ANDROID_ROOT="$PRELOADER_ROOT"
cmake --build build
