@echo off

if not exist build mkdir build
pushd build
cl -MT -nologo -Gm- -GR- -EHa- -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -Z7 -FC ..\code\win32_hh.cpp /link -opt:ref User32.lib Gdi32.lib
popd

exit /b %ERRORLEVEL%
