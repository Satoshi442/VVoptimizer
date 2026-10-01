param(
  [string]$Ndk = "",
  [string]$PreloaderRoot = ""
)
$ErrorActionPreference = "Stop"
if (-not $Ndk) {
  $Ndk = $env:ANDROID_NDK_HOME
}
if (-not $Ndk) { throw "Pass -Ndk or set ANDROID_NDK_HOME." }
if (-not $PreloaderRoot) {
  $PreloaderRoot = Join-Path $PSScriptRoot "preloader-android"
}
$toolchain = Join-Path $Ndk "build\cmake\android.toolchain.cmake"
cmake -S $PSScriptRoot -B (Join-Path $PSScriptRoot "build") -G Ninja `
  "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
  "-DANDROID_ABI=arm64-v8a" `
  "-DANDROID_PLATFORM=android-24" `
  "-DPRELOADER_ANDROID_ROOT=$PreloaderRoot"
cmake --build (Join-Path $PSScriptRoot "build")
