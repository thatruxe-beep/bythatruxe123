#!/bin/sh
# Build Archangel Injector (32-bit Windows GUI exe) with the Zig cross compiler.
# Usage: ./build.sh [zig-path]
set -e
cd "$(dirname "$0")"
ZIG="${1:-/home/user/toolchain/lib/python3.11/site-packages/ziglang/zig}"
[ -x "$ZIG" ] || ZIG="$(command -v zig)"
OUT=out
mkdir -p "$OUT"

"$ZIG" cc -target x86-windows-gnu -O2 -std=c++17 -fno-exceptions -fno-rtti \
  -Wl,--subsystem,windows \
  -o "$OUT/Injector.exe" \
  src/main.cpp src/cxa_guard_shim.cpp \
  ../archangel/imgui/imgui.cpp ../archangel/imgui/imgui_draw.cpp \
  ../archangel/imgui/imgui_tables.cpp ../archangel/imgui/imgui_widgets.cpp \
  ../archangel/imgui/backends/imgui_impl_dx9.cpp \
  ../archangel/imgui/backends/imgui_impl_win32.cpp \
  -I. -I../archangel/imgui -I../archangel/imgui/backends \
  -luser32 -lgdi32 -ld3d9 -ldxguid -ldwmapi -lpsapi -lshell32 -lkernel32

echo "OK: $OUT/Injector.exe"
ls -la "$OUT/Injector.exe"
