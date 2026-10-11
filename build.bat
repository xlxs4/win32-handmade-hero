@echo off

if not exist build mkdir build
pushd build
cl -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -Zi -FC ..\code\win32_hh.cpp User32.lib Gdi32.lib
popd
