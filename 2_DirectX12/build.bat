@echo off
where cl >nul 2>nul
if errorlevel 1 (
  call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
)
cl /EHsc /DUNICODE /D_UNICODE rotating_triangle_directx12.cpp user32.lib dxgi.lib d3d12.lib d3dcompiler.lib
