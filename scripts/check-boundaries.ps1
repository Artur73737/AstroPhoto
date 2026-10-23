$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$violations = @()
foreach ($module in @('core','ui','infrastructure','configuration','app')) {
    $source = Join-Path $projectRoot "src/$module"
    $violations += @(Get-ChildItem -LiteralPath $source -Recurse -File | Select-String -Pattern '#\s*include\s*[<"]EDSDK')
}
$violations += @(Get-ChildItem -LiteralPath (Join-Path $projectRoot 'src/core') -Recurse -File | Select-String -Pattern '#\s*include\s*[<"](imgui|windows|GLFW|astro/Eds)')
if ($violations.Count -gt 0) { $violations | Write-Output; throw 'Dipendenza non ammessa tra moduli' }
Write-Output 'Confini verificati: core indipendente da UI/SDK; header EDSDK confinati nel wrapper.'
