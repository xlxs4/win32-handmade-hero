@echo off

set CommonCompilerFlags=-MT -nologo -Gm- -GR- -EHa- -Oi -WX -W4 -wd4456 -wd4201 -wd4100 -wd4189 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -Z7 -FC
set CommonLinkerFlags=-incremental:no -opt:ref user32.lib gdi32.lib winmm.lib

:: Generate timestamp
for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss_fff"') do set TS=%%i

if not exist build mkdir build
pushd build

:: Game DLL
:: Stale pdbs from previous runs
del hh_*.pdb > NUL 2> NUL
:: We use the timestamp to generate a unique pdb every time, because Visual Studio debugger locks a pdb,
:: and doesn't free it after a FreeLibrary call
cl %CommonCompilerFlags% ..\code\hh.cpp -Fmhh.map -LD /link -incremental:no /PDB:hh_%TS%.pdb /EXPORT:GameGetSoundSamples /EXPORT:GameUpdateAndRender
if errorlevel 1 (popd & exit /b 1)

:: Platform exe
cl %CommonCompilerFlags% ..\code\win32_hh.cpp -Fmwin32_hh.map /link %CommonLinkerFlags%
popd

exit /b %ERRORLEVEL%
