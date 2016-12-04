@echo off

if not exist ..\build mkdir ..\build
pushd ..\build

REM Getting Started Chapter
cl /Od /Zi /nologo /wd4577 ..\code\win32_gettingstarted.cpp /link user32.lib Gdi32.lib DSound.lib Winmm.lib Opengl32.lib

REM Lighting Chapter
cl /Od /Zi /nologo /wd4577 ..\code\win32_lighting.cpp /link user32.lib Gdi32.lib DSound.lib Winmm.lib Opengl32.lib

REM Model Chapter
cl /Od /Zi /EHsc /nologo /wd4577 ..\code\win32_model.cpp /link user32.lib Gdi32.lib DSound.lib Winmm.lib Opengl32.lib

popd
