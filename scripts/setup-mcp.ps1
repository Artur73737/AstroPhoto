param(
    [ValidateSet('http','stdio')][string]$Transport='http',
    [string]$Executable='',
    [int]$Port=47653
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
if(!$Executable){$Executable=if(Test-Path -LiteralPath (Join-Path $projectRoot 'AstroCanonController.exe')){Join-Path $projectRoot 'AstroCanonController.exe'}else{Join-Path $projectRoot 'out/Release/AstroCanonController.exe'}}
$Executable=(Resolve-Path -LiteralPath $Executable).Path
if([version](Get-Item -LiteralPath $Executable).VersionInfo.FileVersion -lt [version]'0.4.0'){throw 'MCP requires AstroPhoto 0.4.0 or newer'}
if($Port -lt 1024 -or $Port -gt 65535){throw 'Port must be 1024..65535'}
if(Get-Process AstroCanonController -ErrorAction SilentlyContinue){throw 'Close AstroPhoto before switching/configuring the MCP transport.'}
$init=Start-Process -FilePath $Executable -ArgumentList '--mcp-init' -WindowStyle Hidden -PassThru -Wait
if($init.ExitCode){throw 'Native MCP initialization failed'}
$settingsPath=Join-Path $env:LOCALAPPDATA 'AstroPhoto/MCP/settings.json'
$settings=Get-Content -LiteralPath $settingsPath -Raw | ConvertFrom-Json -AsHashtable
$settings.transport=$Transport
$settings.port=$Port
[IO.File]::WriteAllText($settingsPath,($settings | ConvertTo-Json),[Text.UTF8Encoding]::new($false))
$codexDirectory=if($env:CODEX_HOME){$env:CODEX_HOME}else{Join-Path $env:USERPROFILE '.codex'}
$configPath=Join-Path $codexDirectory 'config.toml'
if(Test-Path -LiteralPath $configPath){Copy-Item -LiteralPath $configPath -Destination (Join-Path $codexDirectory ('config.before-astrophoto-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfff')+'.toml'))}
# Let Codex parse and edit its own TOML; never rewrite unrelated sections.
foreach($name in @('astrophoto','astrophoto_http','astrophoto_stdio')){
    & codex mcp remove $name
    if($LASTEXITCODE){throw "Could not remove previous MCP entry: $name"}
}
if($Transport -eq 'stdio'){
    # Compatibility override for older CLI builds that reject the desktop's
    # service_tier="default". -c affects this invocation only, not saved preferences.
    & codex -c 'service_tier="fast"' mcp add astrophoto -- $Executable --mcp-stdio
}else{
    & codex -c 'service_tier="fast"' mcp add astrophoto --url "http://127.0.0.1:$Port/mcp"
}
if($LASTEXITCODE){throw 'Codex MCP registration failed'}
if($Transport -eq 'http'){
    # The section is newly created by Codex, so it cannot contain old headers.
    $headerValue=('Bearer '+$settings.token) | ConvertTo-Json -Compress
    [IO.File]::AppendAllText($configPath,"`n[mcp_servers.astrophoto.http_headers]`nAuthorization = $headerValue`n",[Text.UTF8Encoding]::new($false))
    Start-Process -FilePath $Executable -ArgumentList '--mcp-http' -WorkingDirectory (Split-Path -Parent $Executable) -WindowStyle Hidden | Out-Null
}
Write-Output "Configured ONE Codex server: astrophoto ($Transport). Restart Codex. No Python runtime or sidecar is used."
