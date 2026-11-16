param([switch] $SkipBuild)

$ErrorActionPreference = 'Stop'
$RepoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$OutputDirectory = Join-Path $RepoRoot 'Build\Tests\ParticleIntegration'
$VsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$VsRoot = & $VsWhere -latest -prerelease -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $VsRoot) { throw 'Visual Studio C++ build tools were not found.' }
$MsBuild = Join-Path $VsRoot 'MSBuild\Current\Bin\MSBuild.exe'
$DevCommand = Join-Path $VsRoot 'Common7\Tools\VsDevCmd.bat'
$BinaryDirectory = Join-Path $RepoRoot 'Build\Bin\Debug-windows-x86_64'
$EngineLibrary = Join-Path $BinaryDirectory 'HitoriEngine.lib'
$FbxRoot = $env:FBX_SDK_ROOT
if (-not $FbxRoot) { $FbxRoot = Join-Path $RepoRoot 'Source\ThirdParty\FBX\2020.2' }
$VcpkgRoot = Join-Path $RepoRoot 'Intermediate\vcpkg_installed\x64-windows'

Push-Location $RepoRoot
try {
    if (-not $SkipBuild) {
        & $MsBuild 'Source\Runtime\HitoriEngine.vcxproj' /m /p:Configuration=Debug /p:Platform=x64 /verbosity:minimal
        if ($LASTEXITCODE -ne 0) { throw 'Engine build failed.' }
    }
    if (-not (Test-Path -LiteralPath $EngineLibrary)) { throw 'Build HitoriEngine Debug x64 before running the tests.' }
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
    $Executable = Join-Path $OutputDirectory 'ParticleIntegrationTests.exe'
    $ResponseFile = Join-Path $OutputDirectory 'compile.rsp'
    $Arguments = @('/nologo', '/std:c++20', '/EHsc', '/MDd', '/Od', '/Zi', '/bigobj', '/utf-8',
        '/DWIN32_LEAN_AND_MEAN', '/DNOMINMAX', '/DUNICODE', '/D_UNICODE', '/DENGINE_PLATFORM_WINDOWS', '/DENGINE_DEBUG', '/D_DEBUG', '/DWITH_FBX_SDK=1')
    foreach ($Include in @('Source\Runtime', 'Source\ThirdParty\json', 'Source\ThirdParty\stb', 'Build\Generated\UHT\Debug-windows-x86_64\Runtime')) {
        $Arguments += '/I"' + (Join-Path $RepoRoot $Include) + '"'
    }
    $Arguments += '/I"' + (Join-Path $VcpkgRoot 'include') + '"'
    $Arguments += '/I"' + (Join-Path $VcpkgRoot 'include\luajit') + '"'
    $Arguments += '/I"' + (Join-Path $FbxRoot 'include') + '"'
    $Arguments += '"' + (Join-Path $PSScriptRoot 'ParticleIntegrationTests.cpp') + '"'
    $Arguments += '"' + (Join-Path $PSScriptRoot 'ParticleSimulationTests.cpp') + '"'
    $Arguments += '"' + (Join-Path $PSScriptRoot 'ParticleLoaderTests.cpp') + '"'
    $Arguments += '/Fo"' + $OutputDirectory.Replace('\', '/') + '/"'
    $Arguments += '/Fd"' + (Join-Path $OutputDirectory 'ParticleIntegrationTests.pdb') + '"'
    $Arguments += '/Fe"' + $Executable + '"'
    $Arguments += '/link'
    $Arguments += '/WHOLEARCHIVE:"' + $EngineLibrary + '"'
    $Arguments += '/LIBPATH:"' + (Join-Path $VcpkgRoot 'debug\lib') + '"'
    $Arguments += '/LIBPATH:"' + (Join-Path $FbxRoot 'lib\vs2017\x64\debug') + '"'
    $Arguments += @('lua51.lib', 'd3d11.lib', 'dxgi.lib', 'dxguid.lib', 'd3dcompiler.lib', 'libfbxsdk-md.lib', 'libxml2-md.lib', 'zlib-md.lib', 'wininet.lib', 'ws2_32.lib', 'user32.lib', 'gdi32.lib', 'shell32.lib', 'ole32.lib', 'advapi32.lib', 'comdlg32.lib', 'winmm.lib')
    [IO.File]::WriteAllText($ResponseFile, ($Arguments -join ' '), [Text.UTF8Encoding]::new($false))
    & $env:ComSpec /d /s /c "call `"$DevCommand`" -no_logo -arch=x64 -host_arch=x64 && cl.exe @`"$ResponseFile`""
    if ($LASTEXITCODE -ne 0) { throw 'Particle integration test compilation failed.' }
    Copy-Item -LiteralPath (Join-Path $VcpkgRoot 'debug\bin\lua51.dll') -Destination $OutputDirectory -Force
    & $Executable
    if ($LASTEXITCODE -ne 0) { throw 'Particle integration tests failed.' }
}
finally { Pop-Location }
