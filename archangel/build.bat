@echo off
rem Build Archangel.dll (32-bit Windows DLL) with MSVC x86.
rem Run from a "x86 Native Tools Command Prompt" (Visual Studio).
setlocal
cd /d %~dp0
where cl >nul 2>nul || (echo MSVC cl.exe not found - use the x86 Native Tools prompt & exit /b 1)

if not exist out mkdir out

python -c "data=open('payload/archangel.lua','rb').read();f=open('src/payload_lua.h','w');f.write('// auto-generated\n#pragma once\nstatic const unsigned char g_payloadLua[] = {\n');[f.write(' '+','.join(str(b) for b in data[i:i+16])+',\n') for i in range(0,len(data),16)];f.write('};\nstatic const unsigned int g_payloadLuaSize = %d;\n'%len(data))"
if errorlevel 1 (echo payload header generation failed & exit /b 1)

cl /nologo /MD /O2 /EHsc- /GR- /DNDEBUG /Fo"out\\" /Fd"out\" ^
  src\config.cpp src\keycheck.cpp src\lua_mta.cpp src\dx9hook.cpp src\menu.cpp src\esp.cpp src\dllmain.cpp ^
  imgui\imgui.cpp imgui\imgui_draw.cpp imgui\imgui_tables.cpp imgui\imgui_widgets.cpp ^
  imgui\backends\imgui_impl_dx9.cpp imgui\backends\imgui_impl_win32.cpp ^
  /I. /Iimgui /Iimgui\backends ^
  /link /DLL /OUT:out\DLL.dll user32.lib gdi32.lib d3d9.lib winhttp.lib psapi.lib

if errorlevel 1 (echo BUILD FAILED & exit /b 1)
echo OK: out\DLL.dll
endlocal
