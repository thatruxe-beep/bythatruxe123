#!/bin/sh
# Build Archangel.dll (32-bit Windows DLL) with the Zig cross compiler (no Windows needed).
# Usage: ./build.sh [zig-path]
set -e
cd "$(dirname "$0")"
ZIG="${1:-/home/user/toolchain/lib/python3.11/site-packages/ziglang/zig}"
[ -x "$ZIG" ] || ZIG="$(command -v zig)"
OUT=out
mkdir -p "$OUT"

# regenerate embedded payload header
python3 - "$ZIG" <<'EOF'
import sys
data = open('payload/archangel.lua','rb').read()
with open('src/payload_lua.h','w') as f:
    f.write('// auto-generated from payload/archangel.lua - do not edit\n#pragma once\nstatic const unsigned char g_payloadLua[] = {\n')
    for i in range(0, len(data), 16):
        f.write(' ' + ','.join(str(b) for b in data[i:i+16]) + ',\n')
    f.write('};\nstatic const unsigned int g_payloadLuaSize = %d;\n' % len(data))
print("payload_lua.h regenerated:", len(data), "bytes")
EOF

"$ZIG" cc -target x86-windows-gnu -shared -O2 -std=c++17 -fno-exceptions -fno-rtti \
  -o "$OUT/DLL.dll" \
  src/config.cpp src/keycheck.cpp src/lua_mta.cpp src/dx9hook.cpp src/menu.cpp src/esp.cpp src/authorization.cpp src/dllmain.cpp src/cxa_guard_shim.cpp \
  imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp \
  imgui/backends/imgui_impl_dx9.cpp imgui/backends/imgui_impl_win32.cpp \
  -I. -Iimgui -Iimgui/backends \
  -luser32 -lgdi32 -ld3d9 -lwinhttp -lpsapi -ldwmapi -lkernel32

echo "OK: $OUT/DLL.dll"
ls -la "$OUT/DLL.dll"
