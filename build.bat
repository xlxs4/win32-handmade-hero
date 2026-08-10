@echo off

if not exist build mkdir build
pushd build
cl -Zi -FC ..\code\win32_hh.cpp User32.lib Gdi32.lib
popd
