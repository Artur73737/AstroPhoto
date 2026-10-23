$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content -LiteralPath (Join-Path $projectRoot 'external/edsdk-manifest.json') -Raw | ConvertFrom-Json
$sdkRoot = Join-Path $projectRoot 'external/EDSDK'
foreach ($file in $manifest.files) {
    $destination = Join-Path $sdkRoot $file.path
    if ((Test-Path -LiteralPath $destination) -and (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -eq $file.sha256) { continue }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
    $temporary = $destination + '.download'
    try {
        Invoke-WebRequest -Uri $file.url -OutFile $temporary
        if ((Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash -ne $file.sha256) { throw "SHA256 non corrispondente: $($file.path)" }
        Move-Item -LiteralPath $temporary -Destination $destination -Force
    } finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary }
    }
}
Write-Output "Canon EDSDK $($manifest.version) $($manifest.architecture): file verificati in $sdkRoot"
