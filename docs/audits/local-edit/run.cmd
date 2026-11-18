@echo off
setlocal
if not defined VCToolsInstallDir (
  call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
  if errorlevel 1 exit /b 1
)
cd /d "%~dp0"
cl /nologo /std:c++17 /O2 /EHsc /utf-8 /I..\..\..\core\include local-edit.cpp /Fe:local-current.exe
if errorlevel 1 exit /b 1
pushd ..
node fixed-header.mjs
if errorlevel 1 exit /b 1
popd
cl /nologo /std:c++17 /O2 /EHsc /utf-8 /I..\fixed local-edit.cpp /Fe:local-fixed.exe
if errorlevel 1 exit /b 1
for %%P in (2 4) do (
  for %%C in (current fixed) do (
    local-%%C.exe %%P > slots-%%P-%%C.csv 2> rng-%%P-%%C.txt
    if errorlevel 1 exit /b 1
  )
)
node summarize.mjs
