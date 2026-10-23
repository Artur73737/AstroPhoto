param()
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$dependencies = @(
    @{ Name='imgui'; Url='https://github.com/ocornut/imgui.git'; Tag='v1.91.9b-docking'; Commit='4806a1924ff6181180bf5e4b8b79ab4394118875' },
    @{ Name='glfw'; Url='https://github.com/glfw/glfw.git'; Tag='3.4'; Commit='7b6aead9fb88b3623e3b3725ebb42670cbe4c579' },
    @{ Name='googletest'; Url='https://github.com/google/googletest.git'; Tag='v1.16.0'; Commit='6910c9d9165801d8827d628cb72eb7ea9dd538c5' },
    @{ Name='libraw'; Url='https://github.com/LibRaw/LibRaw.git'; Tag='0.22.2'; Commit='b93f6e45c194f5df9b02a43b1af9a54b4f41f33f' },
    @{ Name='json'; Url='https://github.com/nlohmann/json.git'; Tag='v3.11.3'; Commit='9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03' }
)
foreach ($dep in $dependencies) {
    $target = Join-Path $projectRoot "external/$($dep.Name)"
    if (!(Test-Path -LiteralPath $target)) {
        & git clone --depth 1 --branch $dep.Tag $dep.Url $target
        if ($LASTEXITCODE -ne 0) { throw "Download fallito: $($dep.Name)" }
    }
    $actual = & git -C $target rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $actual -ne $dep.Commit) { throw "Revisione inattesa: $($dep.Name). Non modifico la cartella esistente." }
    Write-Output "$($dep.Name): $actual"
}
Write-Output 'Dipendenze pubbliche pronte. Per verificare/ripristinare EDSDK: scripts/install-edsdk.ps1.'
