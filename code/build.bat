@echo off

if not exist ..\build mkdir ..\build
pushd ..\build

cl /Od /Zi /nologo ..\code\win32_aqcube.cpp /link user32.lib Gdi32.lib DSound.lib Winmm.lib Opengl32.lib

popd
