param([switch]$Connect)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$appPath = Join-Path $projectRoot 'out/Release/AstroCanonController.exe'
if (!(Test-Path -LiteralPath $appPath)) { $appPath = Join-Path $projectRoot 'out/build/Release/AstroCanonController.exe' }
if (!(Test-Path -LiteralPath $appPath)) { $appPath = Join-Path $projectRoot 'out/build-editor/Release/AstroCanonController.exe' }
if (!(Test-Path -LiteralPath $appPath)) { $appPath = Join-Path $projectRoot 'build-canon/Release/AstroCanonController.exe' }
if (!(Test-Path -LiteralPath $appPath)) { $appPath = Join-Path $projectRoot 'build/Release/AstroCanonController.exe' }
if (!(Test-Path -LiteralPath $appPath)) { throw 'Eseguire prima scripts/build.ps1' }
if ($Connect) { Start-Process -FilePath $appPath -WorkingDirectory (Split-Path -Parent $appPath) -ArgumentList '--connect' }
else { Start-Process -FilePath $appPath -WorkingDirectory (Split-Path -Parent $appPath) }
