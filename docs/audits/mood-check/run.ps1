param([string] $BuildDir = "$PSScriptRoot/../../../build")

$ErrorActionPreference = 'Stop'
$build = (Resolve-Path -LiteralPath $BuildDir).Path
$vs = "$env:ProgramFiles/Microsoft Visual Studio/2022/Community"
$vcvars = "$vs/VC/Auxiliary/Build/vcvars64.bat"
[xml] $project = Get-Content -Raw "$build/chordslots_plugin_tests.vcxproj"
$release = $project.Project.ItemDefinitionGroup | Where-Object { $_.Condition -like '*Release|x64*' -and $_.Condition -notlike '*MinSizeRel*' } | Select-Object -First 1
$compile = $release.ClCompile | Select-Object -First 1
$definitions = ([string] $compile.PreprocessorDefinitions -split ';') |
    Where-Object { $_ -and $_ -notlike '%(*' } | ForEach-Object { "/D$_" }
$includes = ([string] $compile.AdditionalIncludeDirectories -split ';') |
    Where-Object { $_ -and $_ -notlike '%(*' } | ForEach-Object { '/I"' + $_ + '"' }
$libraries = ([string] $release.Link.AdditionalDependencies -split ';') | ForEach-Object {
    if ($_ -like '*\*') { '"' + (Join-Path $build $_) + '"' } else { $_ }
}
Push-Location $PSScriptRoot
try {
    @('/nologo /std:c++17 /O2 /MD /EHsc /utf-8 /bigobj', $definitions, $includes,
      'check.cpp /Fe:check.exe /link', $libraries) | Set-Content -Encoding ascii compile.rsp
    & cmd.exe /d /c ('call "' + $vcvars + '" && cl @compile.rsp')
    if ($LASTEXITCODE) { throw "Compilation failed ($LASTEXITCODE)" }
    & .\check.exe
    if ($LASTEXITCODE) { throw "Editor check failed ($LASTEXITCODE)" }
} finally {
    Pop-Location
}
