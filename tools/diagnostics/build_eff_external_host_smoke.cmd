@echo off
setlocal
pushd "%~dp0..\.."
set "eff_smoke_vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
where cl >nul 2>nul
if errorlevel 1 for /f "usebackq tokens=*" %%i in (`"%eff_smoke_vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin\external-loader-smoke mkdir bin\external-loader-smoke
cl /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /Fo"bin\external-loader-smoke\host.obj" tools\diagnostics\eff_external_host_smoke.cpp /Fe"bin\external-loader-smoke\EFFTestHost.exe"
set "eff_smoke_result=%errorlevel%"
popd
exit /b %eff_smoke_result%
