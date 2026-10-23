param([switch]$WithEDSDK, [switch]$HardwareTests, [ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $projectRoot $(if ($WithEDSDK) { 'out/build' } else { 'out/build-editor' })
& (Join-Path $PSScriptRoot 'bootstrap.ps1')
$sdkFlag = if ($WithEDSDK) { 'ON' } else { 'OFF' }
$testFlag = if ($HardwareTests) { 'ON' } else { 'OFF' }
& cmake -S $projectRoot -B $buildRoot -G 'Visual Studio 17 2022' -A x64 "-DASTRO_WITH_EDSDK=$sdkFlag" "-DASTRO_HARDWARE_TESTS=$testFlag"
if ($LASTEXITCODE -ne 0) { throw 'Configurazione CMake fallita' }
& cmake --build $buildRoot --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Compilazione fallita' }
& ctest --test-dir $buildRoot -C $Configuration --output-on-failure -LE hardware
if ($LASTEXITCODE -ne 0) { throw 'Test falliti' }
& (Join-Path $PSScriptRoot 'check-boundaries.ps1')
Write-Output "Eseguibile: $buildRoot/$Configuration/AstroCanonController.exe"
