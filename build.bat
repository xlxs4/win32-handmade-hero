@echo off

set CommonCompilerFlags=-MT -nologo -Gm- -GR- -EHa- -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -Z7 -FC
set CommonLinkerFlags=-opt:ref user32.lib gdi32.lib winmm.lib

if not exist build mkdir build
pushd build
cl %CommonCompilerFlags% ..\code\win32_hh.cpp /link %CommonLinkerFlags%
popd

exit /b %ERRORLEVEL%
