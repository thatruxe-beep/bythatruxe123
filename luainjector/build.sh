#!/bin/sh
# Build Archangel Lua Injector (minimal 32-bit Windows DLL, no ImGui) with Zig.
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
data = open('payload/script.lua','rb').read()
with open('src/payload_lua.h','w') as f:
    f.write('// auto-generated from payload/script.lua - do not edit\n#pragma once\nstatic const unsigned char g_payloadLua[] = {\n')
    for i in range(0, len(data), 16):
        f.write(' ' + ','.join(str(b) for b in data[i:i+16]) + ',\n')
    f.write('};\nstatic const unsigned int g_payloadLuaSize = %d;\n' % len(data))
print("payload_lua.h regenerated:", len(data), "bytes")
EOF

"$ZIG" cc -target x86-windows-gnu -shared -O2 -std=c++17 -fno-exceptions -fno-rtti \
  -o "$OUT/luainjector.dll" \
  src/main.cpp \
  -I. \
  -lkernel32

echo "OK: $OUT/luainjector.dll"
ls -la "$OUT/luainjector.dll"
