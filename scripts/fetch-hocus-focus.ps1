$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$destination=Join-Path $root 'external/hocus-focus'
$revision='bced3c30d8a86114e2578039149667242737b942'
if(!(Test-Path -LiteralPath $destination)) {
    & git clone 'https://github.com/ghilios/hocus-focus.git' $destination
    if($LASTEXITCODE -ne 0){throw 'Hocus Focus clone failed'}
    & git -C $destination checkout --detach $revision
    if($LASTEXITCODE -ne 0){throw 'Pinned Hocus Focus revision unavailable'}
}
$actual=& git -C $destination rev-parse HEAD
if($LASTEXITCODE -ne 0 -or $actual -ne $revision){throw 'Existing Hocus Focus checkout differs from the pinned reference; left unchanged'}
$changes=& git -C $destination status --porcelain
if($changes){throw 'Hocus Focus reference contains local changes; left unchanged'}
Write-Output "Hocus Focus reference verified: $destination ($revision)"
