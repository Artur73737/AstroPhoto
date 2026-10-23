param([string]$BuildDirectory='out/build', [string]$Version='0.2.0')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$build=Join-Path $root "$BuildDirectory/Release"
$out=Join-Path $root 'out'
$stage=Join-Path $out 'package/payload'
$release=Join-Path $out 'Release'
$work=Join-Path $out 'package'
$wix=Join-Path $root 'external/wix'
if($Version -notmatch '^\d+\.\d+\.\d+$'){throw 'Version must be major.minor.patch'}
if(!(Test-Path -LiteralPath "$build/AstroCanonController.exe")){throw 'Build Release with EDSDK first'}
if(Test-Path -LiteralPath $stage){
    $resolved=(Resolve-Path -LiteralPath $stage).Path
    if($resolved -ne [IO.Path]::GetFullPath((Join-Path $root 'out/package/payload'))){throw 'Invalid staging path'}
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
New-Item -ItemType Directory -Force $stage,$release,$work,$wix | Out-Null
foreach($name in @('AstroCanonController.exe','EDSDK.dll','EdsImage.dll','defaults.json','Cousine-Regular.ttf')){
    Copy-Item -LiteralPath (Join-Path $build $name) -Destination $stage
}
Copy-Item -LiteralPath "$build/licenses" -Destination $stage -Recurse
foreach($item in @(@('imgui','external/imgui/LICENSE.txt'),@('glfw','external/glfw/LICENSE.md'),@('json','external/json/LICENSE.MIT'))){
    $dest=Join-Path $stage "licenses/$($item[0])"
    New-Item -ItemType Directory -Force $dest | Out-Null
    Copy-Item -LiteralPath (Join-Path $root $item[1]) -Destination $dest
}
Copy-Item -LiteralPath "$root/README.md" -Destination $stage
Copy-Item -LiteralPath "$root/assets/astrophoto.ico" -Destination $stage
# App-local Microsoft runtime from the installed Visual Studio redistributable.
$vswhere="${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$installations=& $vswhere -all -products '*' -property installationPath
$runtime=@(foreach($installation in $installations){
    Get-ChildItem -Path "$installation/VC/Redist/MSVC/*/x64/Microsoft.VC*.CRT" -Directory -ErrorAction SilentlyContinue
}) | Sort-Object { [version]$_.Parent.Parent.Name } -Descending | Select-Object -First 1
if(!$runtime){throw 'Visual C++ x64 redistributable not found'}
Get-ChildItem -LiteralPath $runtime.FullName -Filter '*.dll' | Copy-Item -Destination $stage
if(!(Test-Path -LiteralPath "$wix/candle.exe")){
    $zip=Join-Path $wix 'wix314-binaries.zip'
    Invoke-WebRequest 'https://github.com/wixtoolset/wix3/releases/download/wix3141rtm/wix314-binaries.zip' -OutFile $zip
    if((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash -ne '6AC824E1642D6F7277D0ED7EA09411A508F6116BA6FAE0AA5F2C7DAA2FF43D31'){throw 'WiX archive hash mismatch'}
    Expand-Archive -LiteralPath $zip -DestinationPath $wix -Force
}
function Escape-Xml([string]$value){[Security.SecurityElement]::Escape($value)}
function Stable-Id([string]$value){
    $sha=[Security.Cryptography.SHA256]::Create()
    try{'id'+([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($value))).Replace('-','').Substring(0,24))}finally{$sha.Dispose()}
}
function Stable-Guid([string]$value){
    $sha=[Security.Cryptography.SHA256]::Create()
    try{([guid]([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes("AstroPhoto-x64/$value"))).Replace('-','').Substring(0,32))).ToString()}finally{$sha.Dispose()}
}
$components=[Collections.Generic.List[string]]::new()
$directories=[Collections.Generic.List[string]]::new()
function Directory-Xml([string]$directory,[string]$relative){
    $xml=[Text.StringBuilder]::new()
    foreach($file in Get-ChildItem -LiteralPath $directory -File | Sort-Object Name){
        $path=if($relative){"$relative/$($file.Name)"}else{$file.Name}
        $id=Stable-Id $path
        $components.Add($id)
        $fileId=if($path -eq 'AstroCanonController.exe'){'AppExe'}else{"file_$id"}
        [void]$xml.AppendLine("<Component Id=`"$id`" Guid=`"$(Stable-Guid $path)`" Win64=`"yes`"><File Id=`"$fileId`" Source=`"$(Escape-Xml $file.FullName)`" />")
        [void]$xml.AppendLine("<RegistryValue Root=`"HKCU`" Key=`"Software\AstroPhoto\Installer\Files`" Name=`"$id`" Type=`"integer`" Value=`"1`" KeyPath=`"yes`" />")
        [void]$xml.AppendLine("<RemoveFolder Id=`"remove_$id`" On=`"uninstall`" /></Component>")
    }
    foreach($dir in Get-ChildItem -LiteralPath $directory -Directory | Sort-Object Name){
        $path=if($relative){"$relative/$($dir.Name)"}else{$dir.Name}
        $directories.Add("dir_$(Stable-Id $path)")
        [void]$xml.AppendLine("<Directory Id=`"dir_$(Stable-Id $path)`" Name=`"$(Escape-Xml $dir.Name)`">")
        [void]$xml.AppendLine((Directory-Xml $dir.FullName $path))
        [void]$xml.AppendLine('</Directory>')
    }
    $xml.ToString()
}
$payloadXml=Directory-Xml $stage ''
$removeDirectories=($directories | ForEach-Object {"<RemoveFolder Id=`"remove_$_`" Directory=`"$_`" On=`"uninstall`" />"}) -join "`n"
$refs=($components | ForEach-Object {"<ComponentRef Id=`"$_`" />"}) -join "`n"
$source=@"
<?xml version="1.0" encoding="UTF-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">
 <Product Id="*" Name="AstroPhoto" Language="1033" Version="$Version" Manufacturer="AstroPhoto" UpgradeCode="037A8396-4C38-40FD-90B4-39312233A6A9">
  <Package InstallerVersion="500" Compressed="yes" InstallScope="perUser" InstallPrivileges="limited" Platform="x64" Description="AstroPhoto Canon camera controller" />
  <MajorUpgrade DowngradeErrorMessage="A newer version of AstroPhoto is already installed." />
  <MediaTemplate EmbedCab="yes" CompressionLevel="high" />
  <Condition Message="AstroPhoto requires 64-bit Windows.">Installed OR VersionNT64</Condition>
  <Icon Id="AppIcon" SourceFile="$(Escape-Xml "$root/assets/astrophoto.ico")" />
  <Property Id="ARPPRODUCTICON" Value="AppIcon" />
  <Property Id="ARPURLINFOABOUT" Value="https://github.com/Artur73737/AstroPhoto" />
  <Property Id="INSTALLFOLDER">
   <RegistrySearch Id="PreviousInstallFolder" Root="HKCU" Key="Software\AstroPhoto\Installer" Name="InstallFolder" Type="raw" Win64="yes" />
  </Property>
  <Directory Id="TARGETDIR" Name="SourceDir">
   <Directory Id="LocalAppDataFolder"><Directory Id="ProgramsFolder" Name="Programs"><Directory Id="INSTALLFOLDER" Name="AstroPhoto">
    $payloadXml
   </Directory></Directory></Directory>
   <Directory Id="ProgramMenuFolder"><Directory Id="AppMenu" Name="AstroPhoto">
    <Component Id="MenuShortcut" Guid="*" Win64="yes">
     <Shortcut Id="StartMenuShortcut" Name="AstroPhoto" Target="[INSTALLFOLDER]AstroCanonController.exe" WorkingDirectory="INSTALLFOLDER" Icon="AppIcon" />
     <RegistryValue Root="HKCU" Key="Software\AstroPhoto\Installer" Name="Shortcut" Type="integer" Value="1" KeyPath="yes" />
     <RegistryValue Root="HKCU" Key="Software\AstroPhoto\Installer" Name="InstallFolder" Type="string" Value="[INSTALLFOLDER]" />
     <RemoveFolder Id="RemoveMenu" On="uninstall" />
     <RemoveFolder Id="RemovePrograms" Directory="ProgramsFolder" On="uninstall" />
     $removeDirectories
    </Component>
   </Directory></Directory>
  </Directory>
  <Feature Id="Main" Title="AstroPhoto" Level="1">$refs<ComponentRef Id="MenuShortcut" /></Feature>
  <Property Id="WIXUI_INSTALLDIR" Value="INSTALLFOLDER" />
  <UIRef Id="WixUI_InstallDir" />
  <WixVariable Id="WixUILicenseRtf" Value="$(Escape-Xml "$root/installer/notice.rtf")" />
 </Product>
</Wix>
"@
$wxs=Join-Path $work 'AstroPhoto.wxs'
Set-Content -LiteralPath $wxs -Value $source -Encoding UTF8
& "$wix/candle.exe" -nologo -arch x64 -out "$work/AstroPhoto.wixobj" $wxs
if($LASTEXITCODE -ne 0){throw 'WiX compilation failed'}
$msi=Join-Path $out "AstroPhoto-$Version-x64.msi"
# ICE91 warns about per-user paths in packages that might install per-machine.
# This package deliberately supports only per-user installation.
& "$wix/light.exe" -nologo -sice:ICE91 -ext WixUIExtension -pdbout "$work/AstroPhoto.wixpdb" -out $msi "$work/AstroPhoto.wixobj"
if($LASTEXITCODE -ne 0){throw 'MSI link/validation failed'}
Get-ChildItem -LiteralPath $stage | Copy-Item -Destination $release -Recurse -Force
Get-FileHash -LiteralPath $msi -Algorithm SHA256 | ForEach-Object {"$($_.Hash.ToLower())  $(Split-Path -Leaf $msi)"} | Set-Content -LiteralPath "$msi.sha256" -Encoding ascii
Write-Output "Portable: $release"
Write-Output "Installer: $msi"
