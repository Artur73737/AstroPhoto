param([string]$Executable='out/build/Release/AstroCanonController.exe',[string]$Artifacts='out/validation/mcp')
$ErrorActionPreference='Stop'
$Executable=(Resolve-Path -LiteralPath $Executable).Path
if(Get-Process AstroCanonController -ErrorAction SilentlyContinue){throw 'Close AstroPhoto before this transport test'}
New-Item -ItemType Directory -Force $Artifacts | Out-Null
$Artifacts=(Resolve-Path -LiteralPath $Artifacts).Path
$settingsPath=Join-Path $env:LOCALAPPDATA 'AstroPhoto/MCP/settings.json'
$init=Start-Process -FilePath $Executable -ArgumentList '--mcp-init' -WindowStyle Hidden -PassThru -Wait
if($init.ExitCode){throw 'MCP init failed'}
$settings=Get-Content -LiteralPath $settingsPath -Raw | ConvertFrom-Json
$url="http://127.0.0.1:$($settings.port)/mcp"
$script:requestId=0
$script:client=$null
$script:mode=''
$script:checks=0
function Assert-That([bool]$condition,[string]$message){if(!$condition){throw $message};$script:checks++}
function Rpc([string]$method,[hashtable]$parameters=@{}){
    $script:requestId++
    $body=@{jsonrpc='2.0';id=$script:requestId;method=$method;params=$parameters} | ConvertTo-Json -Depth 64 -Compress
    if($script:mode -eq 'stdio'){
        $script:client.StandardInput.WriteLine($body)
        $read=$script:client.StandardOutput.ReadLineAsync()
        if(!$read.Wait(15000)){throw 'stdio response timeout'}
        if(!$read.Result){throw ('stdio closed: '+$script:client.StandardError.ReadToEnd())}
        $response=$read.Result | ConvertFrom-Json -AsHashtable -Depth 80
    }else{
        $response=Invoke-RestMethod -Uri $url -Method Post -Headers @{Authorization=('Bearer '+$settings.token);Accept='application/json, text/event-stream';'MCP-Protocol-Version'='2025-11-25'} -ContentType 'application/json' -Body $body -TimeoutSec 15
        $response=$response | ConvertTo-Json -Depth 80 -Compress | ConvertFrom-Json -AsHashtable -Depth 80
    }
    Assert-That ($response.id -eq $script:requestId) 'Mismatched JSON-RPC ID'
    if($response.error){throw ($response.error | ConvertTo-Json -Compress)}
    return $response.result
}
function Tool([string]$name,[hashtable]$arguments=@{},[switch]$ExpectError){
    $result=Rpc 'tools/call' @{name=$name;arguments=$arguments}
    if($ExpectError){Assert-That ($result.isError -eq $true) "Expected tool error: $name"}
    elseif($result.isError){throw "$name failed: $($result.content[0].text)"}
    return $result
}
function Read-Tool([string]$name,[hashtable]$arguments=@{}){return (Tool $name $arguments).structuredContent}
function Wait-Command([long]$id){
    $deadline=[DateTime]::UtcNow.AddSeconds(10)
    do{$r=Read-Tool 'command_status' @{id=$id};if($r.status -ne 'queued'){return $r};Start-Sleep -Milliseconds 50}while([DateTime]::UtcNow -lt $deadline)
    throw 'Command did not complete'
}
$summary=[Collections.Generic.List[object]]::new()
foreach($mode in @('stdio','http')){
    $script:mode=$mode
    $start=[Diagnostics.ProcessStartInfo]::new($Executable,"--mcp-$mode")
    $start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.WorkingDirectory=Split-Path -Parent $Executable
    $start.RedirectStandardInput=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
    $script:client=[Diagnostics.Process]::Start($start)
    $originalConfig=$null;$originalUi=$null
    try{
        if($mode -eq 'http'){
            $ready=$false
            for($i=0;$i -lt 100;$i++){try{$socket=[Net.Sockets.TcpClient]::new();$socket.Connect('127.0.0.1',$settings.port);$socket.Dispose();$ready=$true;break}catch{Start-Sleep -Milliseconds 100}}
            Assert-That $ready 'HTTP listener did not start'
        }
        $hello=Rpc 'initialize' @{protocolVersion='2025-11-25';capabilities=@{};clientInfo=@{name='AstroPhoto native integration test';version='1'}}
        Assert-That ($hello.serverInfo.name -eq 'AstroPhoto') 'Wrong server identity'
        $tools=(Rpc 'tools/list').tools
        Assert-That ($tools.Count -eq 24) 'Incomplete tool catalog'
        $state=Read-Tool 'get_state'
        Assert-That ($state.transport -eq $mode) 'Wrong selected transport'
        Assert-That ($state.pid -eq $script:client.Id) 'MCP is not inside the UI executable'
        Assert-That (($state.threads.Values | Sort-Object -Unique).Count -eq 3) 'UI, core and MCP must have three distinct thread IDs'
        Assert-That (!$state.state.connected) 'Test must not automatically connect the camera'
        if($mode -eq 'stdio'){
            $bound=$false;try{$socket=[Net.Sockets.TcpClient]::new();$socket.Connect('127.0.0.1',$settings.port);$bound=$true}catch{}finally{if($socket){$socket.Dispose()}}
            Assert-That (!$bound) 'HTTP must be OFF in stdio mode'
        }
        $originalConfig=(Read-Tool 'get_settings').config
        $originalUi=$state.ui
        $schema=Read-Tool 'get_control_schema'
        foreach($key in $originalConfig.Keys){if($key -ne 'schema_version'){Assert-That ($schema.settings.properties.ContainsKey($key)) "Missing setting $key"}}
        Tool 'update_settings' @{patch=@{light_theme=(!$originalConfig.light_theme);language='en';star_autofocus=@{min_stars=7}}} | Out-Null
        $changed=(Read-Tool 'get_settings').config
        Assert-That ($changed.language -eq 'en' -and $changed.star_autofocus.min_stars -eq 7) 'Settings patch was not applied'
        Tool 'update_settings' @{patch=@{star_autofocus=@{positions=8}}} -ExpectError | Out-Null
        Assert-That ((Read-Tool 'get_settings').config.star_autofocus.positions -eq $originalConfig.star_autofocus.positions) 'Invalid patch partially applied'
        Tool 'update_ui' @{patch=@{histogram=$false;focus_step=3;view='live';focus_target=@{enabled=$true;u=.25;v=.75};sections=@{af_parameters=$true}}} | Out-Null
        $changed=Read-Tool 'get_state'
        Assert-That ($changed.ui.focus_step -eq 3 -and $changed.ui.sections.af_parameters -and $changed.ui.focus_target.u -eq .25) 'UI controls not synchronized'
        $image=Tool 'get_image' @{source='panel';panel='LiveView'}
        $block=@($image.content | Where-Object type -eq 'image')[0]
        Assert-That ($block.mimeType -eq 'image/png') 'Missing rendered panel image'
        $bytes=[Convert]::FromBase64String($block.data)
        Assert-That ([BitConverter]::ToString($bytes[0..7]) -eq '89-50-4E-47-0D-0A-1A-0A') 'Invalid PNG'
        [IO.File]::WriteAllBytes((Join-Path $Artifacts "$mode-preview.png"),$bytes)
        $layout=Read-Tool 'get_layout'
        Assert-That ($layout.panels.Count -eq 7 -and $layout.monitors.Count -ge 1) 'Incomplete layout'
        Tool 'get_image' @{source='raw_live'} -ExpectError | Out-Null
        Tool 'lens_move' @{step=0} -ExpectError | Out-Null
        $command=Read-Tool 'lens_move' @{step=1}
        $result=Wait-Command $command.id
        Assert-That ($result.status -eq 'failed' -and $result.error) 'Disconnected hardware error was not acknowledged'
        Tool 'get_logs' @{limit=10} | Out-Null
        Tool 'get_analysis' @{source='live'} | Out-Null
        if($mode -eq 'http'){
            $conflictStart=[Diagnostics.ProcessStartInfo]::new($Executable,'--mcp-stdio')
            $conflictStart.UseShellExecute=$false;$conflictStart.CreateNoWindow=$true;$conflictStart.RedirectStandardInput=$true;$conflictStart.RedirectStandardOutput=$true;$conflictStart.RedirectStandardError=$true
            $conflict=[Diagnostics.Process]::Start($conflictStart)
            try{Assert-That ($conflict.WaitForExit(5000)) 'Conflicting transport instance hung';Assert-That ($conflict.ExitCode -ne 0) 'Second transport instance was allowed';Assert-That ($conflict.StandardError.ReadToEnd().Contains('already open')) 'Missing transport exclusivity error'}finally{$conflict.Dispose()}
            $httpClient=[Net.Http.HttpClient]::new()
            try{
                $httpClient.DefaultRequestHeaders.Add('Authorization','Bearer '+$settings.token)
                $httpClient.DefaultRequestHeaders.Add('Accept','application/json, text/event-stream')
                $pending=@(for($i=0;$i -lt 12;$i++){
                    $body=@{jsonrpc='2.0';id=(10000+$i);method='tools/call';params=@{name='get_state';arguments=@{}}} | ConvertTo-Json -Depth 8 -Compress
                    $httpClient.PostAsync($url,[Net.Http.StringContent]::new($body,[Text.Encoding]::UTF8,'application/json'))
                })
                foreach($task in $pending){
                    Assert-That ($task.Wait(15000)) 'Concurrent HTTP request hung'
                    $reply=$task.Result.Content.ReadAsStringAsync().Result | ConvertFrom-Json -AsHashtable -Depth 80
                    Assert-That (!$reply.result.isError) 'Concurrent HTTP tool failed'
                    Assert-That ($reply.result.structuredContent.threads.mcp -eq $state.threads.mcp) 'HTTP created an extra MCP worker'
                }
                $after=Read-Tool 'get_state'
                Assert-That ($after.ui_frame -gt $state.ui_frame -and $after.core_tick -gt $state.core_tick) 'UI/core did not progress independently'
            }finally{$httpClient.Dispose()}
            $unauthorized=Invoke-WebRequest -Uri $url -Method Post -ContentType 'application/json' -Body '{}' -SkipHttpErrorCheck
            Assert-That ($unauthorized.StatusCode -eq 401) 'HTTP requires authentication'
            $origin=Invoke-WebRequest -Uri $url -Method Post -Headers @{Authorization=('Bearer '+$settings.token);Origin='https://example.com'} -ContentType 'application/json' -Body '{}' -SkipHttpErrorCheck
            Assert-That ($origin.StatusCode -eq 403) 'Untrusted Origin accepted'
            $get=Invoke-WebRequest -Uri $url -Headers @{Authorization=('Bearer '+$settings.token)} -SkipHttpErrorCheck
            Assert-That ($get.StatusCode -eq 405) 'Unsupported SSE GET must return 405'
        }
        $summary.Add(@{transport=$mode;tools=$tools.Count;threads=$state.threads;pid=$state.pid;images="$mode-preview.png"})
    }finally{
        if(!$script:client.HasExited){
            if($originalConfig){$restore=$originalConfig.Clone();$restore.Remove('schema_version');Tool 'update_settings' @{patch=$restore} | Out-Null}
            if($originalUi){$restore=$originalUi.Clone();$restore.Remove('error');$restore.focus_target.Remove('revision');Tool 'update_ui' @{patch=$restore} | Out-Null}
            if($mode -eq 'stdio'){$script:client.StandardInput.Close()}else{Tool 'app_window' @{action='close'} | Out-Null}
            Assert-That ($script:client.WaitForExit(15000)) 'Application shutdown hung'
            Assert-That ($script:client.ExitCode -eq 0) 'Application exited with an error'
        }
        $script:client.Dispose();$script:client=$null
    }
}
@{checks=$script:checks;transports=$summary} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $Artifacts 'result.json')
Write-Output "Passed $script:checks native MCP integration checks. Both transports tested separately; only three application thread roles."
