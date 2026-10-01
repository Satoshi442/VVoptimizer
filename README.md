# VV Optimizer — LeviLaunchroid native mod

This is an original native mod built against the native-mod API documented in
LeviLaunchroid 1.5.25.

## What it does

It hooks the Android NDK asset-loading functions:

- `AAssetManager_open`
- `AAsset_getBuffer`
- `AAsset_read`
- `AAsset_close`

It watches only:

`renderer/platform_config/android/platform_configuration.android.json`

When Bedrock loads that configuration, the mod changes the tier-5 Android
Vibrant Visuals profile to a lower-cost profile using values already present in
the game's own Android renderer configuration:

- 480p target resolution
- bilinear upscaling
- deferred distance 4
- low reflections
- low shadows
- low volumetric fog
- low clouds
- block lights off
- bloom off

The mod does not include or redistribute PandaMine's VVO files.

## Why use the launcher source?

LeviLaunchroid 1.5.25 documents the native lifecycle and hook API:

```text
PL_REGISTER_MOD
    -> load()
    -> enable()
    -> disable()
    -> unload()
```

and:

```cpp
pl::memory::HookHandle
```

This project uses those APIs directly. The launcher itself is not modified.

## Important testing note

This is a native prototype, not a claim that every 26.51 renderer loading path
uses `AAsset_getBuffer()`. It also hooks `AAsset_read()` for the case where the
whole JSON is read in one operation.

If the log does not show:

```text
Watching renderer/platform_config/android/platform_configuration.android.json
```

then Bedrock is resolving the resource through a different asset path and we
should trace that path rather than guessing.

## Build

Use the same `preloader-android` source used by your LeviLaunchroid build.

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24 \
  -DPRELOADER_ANDROID_ROOT=/path/to/preloader-android

cmake --build build
```

Or on Windows:

```powershell
.\build.ps1 -Ndk <NDK> -PreloaderRoot <preloader-android>
```

The resulting library is:

```text
build/libvv_optimizer.so
```

Package:

```text
vv-optimizer/
├── manifest.json
├── libvv_optimizer.so
└── config/
    └── config.json
```

as `vv-optimizer.levipack`.

## Next version

Once this prototype is confirmed on your 26.51 installation, the next step is
to move the expensive settings to runtime controls instead of forcing one
profile. For example:

```text
Performance
Balanced
Quality
Custom
```

with separate control over resolution, shadows, reflections, deferred distance,
fog and block lights.
