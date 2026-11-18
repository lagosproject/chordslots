@echo off
setlocal
if not defined VCToolsInstallDir (
  call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
  if errorlevel 1 exit /b 1
)
cd /d "%~dp0"
cl /nologo /std:c++17 /O2 /EHsc /utf-8 /I..\..\core\include engine.cpp /Fe:engine.exe
if errorlevel 1 exit /b 1
engine.exe > brightness-current.csv
if errorlevel 1 exit /b 1
node fixed-header.mjs
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /EHsc /utf-8 /Ifixed engine.cpp /Fe:engine-fixed.exe
if errorlevel 1 exit /b 1
engine-fixed.exe > brightness-fixed.csv
engine.exe edit > brightness-edit-current.csv
if errorlevel 1 exit /b 1
if errorlevel 1 exit /b 1
engine-fixed.exe edit > brightness-edit-fixed.csv
