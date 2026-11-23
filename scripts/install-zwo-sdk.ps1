param()
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$target=Join-Path $root 'external/zwo'
New-Item -ItemType Directory -Force $target |Out-Null
$archive=Join-Path $target 'sdk.zip'
if(!(Test-Path -LiteralPath $archive)){Invoke-WebRequest 'https://dl.zwoastro.com/software?app=DeveloperCameraSdk&platform=windows86&region=Overseas' -OutFile $archive}
if((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne '2293E356266D3D65E53AF220080DC38E0C0C200D8697109C8460361F3FB111E7'){throw 'ASI SDK archive differs from pinned 1.42; review the upstream update explicitly'}
Expand-Archive -LiteralPath $archive -DestinationPath $target -Force
Expand-Archive -LiteralPath (Join-Path $target 'ASI_Camera_SDK/ASI_Windows_SDK_V1.42.zip') -DestinationPath (Join-Path $target 'windows') -Force
Write-Output 'Official ASI 1.42 Windows SDK ready. Install the native ZWO driver separately on the hardware PC.'
