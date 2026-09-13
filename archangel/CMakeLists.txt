cmake_minimum_required(VERSION 3.16)
project(Archangel CXX)

# Build 32-bit: cmake -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=i686 ...
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Archangel SHARED
    src/config.cpp
    src/keycheck.cpp
    src/lua_mta.cpp
    src/dx9hook.cpp
    src/menu.cpp
    src/esp.cpp
    src/dllmain.cpp
    src/cxa_guard_shim.cpp
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_dx9.cpp
    imgui/backends/imgui_impl_win32.cpp
)

target_include_directories(Archangel PRIVATE . imgui imgui/backends)
target_link_libraries(Archangel PRIVATE user32 gdi32 d3d9 winhttp psapi dwmapi)
set_target_properties(Archangel PROPERTIES OUTPUT_NAME "DLL")
