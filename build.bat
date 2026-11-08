@echo off

set CommonCompilerFlags=-MT -nologo -Gm- -GR- -EHa- -Oi -WX -W4 -wd4456 -wd4201 -wd4100 -wd4189 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -Z7 -FC
set CommonLinkerFlags=-incremental:no -opt:ref user32.lib gdi32.lib winmm.lib

if not exist build mkdir build
pushd build
cl %CommonCompilerFlags% ..\code\hh.cpp -Fmhh.map /LD /link /EXPORT:GameGetSoundSamples /EXPORT:GameUpdateAndRender
if errorlevel 1 (popd & exit /b 1)
cl %CommonCompilerFlags% ..\code\win32_hh.cpp -Fmwin32_hh.map /link %CommonLinkerFlags%
popd

exit /b %ERRORLEVEL%
