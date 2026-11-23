param([string]$BuildDirectory='out/build', [string]$Version='0.10.3')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$build=Join-Path $root "$BuildDirectory/Release"
$out=Join-Path $root 'out'
$stage=Join-Path $out 'package/payload'
$release=Join-Path $out 'Release'
$work=Join-Path $out 'package'
$wix=Join-Path $root 'external/wix'
if($Version -notmatch '^\d+\.\d+\.\d+$'){throw 'Version must be major.minor.patch'}
if(!(Test-Path -LiteralPath "$build/AstroController.exe")){throw 'Build Release with EDSDK first'}
if((Get-Item -LiteralPath "$build/AstroController.exe").VersionInfo.ProductVersion -ne $Version){throw 'Executable and installer versions differ; rebuild Release first'}
if(Test-Path -LiteralPath $stage){
    $resolved=(Resolve-Path -LiteralPath $stage).Path
    if($resolved -ne [IO.Path]::GetFullPath((Join-Path $root 'out/package/payload'))){throw 'Invalid staging path'}
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
New-Item -ItemType Directory -Force $stage,$release,$work,$wix | Out-Null
foreach($name in @('AstroController.exe','EDSDK.dll','EdsImage.dll','defaults.json','Cousine-Regular.ttf')){
    Copy-Item -LiteralPath (Join-Path $build $name) -Destination $stage
}
Copy-Item -LiteralPath (Join-Path $build 'ASICamera2.dll') -Destination $stage
$astroDependencies=Join-Path $stage 'dependencies/astap'
$zwoDriver=Join-Path $stage 'drivers/zwo'
New-Item -ItemType Directory -Force $astroDependencies,$zwoDriver|Out-Null
Copy-Item -LiteralPath "$root/external/astap/astap.exe" -Destination $astroDependencies
Get-ChildItem "$root/external/astap/g05_*.290" |Copy-Item -Destination $astroDependencies
Get-ChildItem "$root/external/astap/w08_*.001" |Copy-Item -Destination $astroDependencies
if(@(Get-ChildItem "$astroDependencies/g05_*.290").Count -ne 290){throw 'Complete ASTAP G05 catalog required'}
$driverArchive=Join-Path $root 'external/zwo-driver/setup.exe'
if((Get-FileHash -LiteralPath $driverArchive -Algorithm SHA256).Hash -ne '3B6086A1B0E89FE5F4B5B5B947F2A35FB093B44E577E8B93D3F53F3DEFF4F72E'){throw 'Unexpected ZWO 3.29 driver package'}
if((Get-AuthenticodeSignature -FilePath "$root/external/zwo-driver/extracted/driver/x64/asicamusb3.cat").Status -ne 'Valid'){throw 'ZWO driver catalog signature invalid'}
Get-ChildItem -LiteralPath "$root/external/zwo-driver/extracted/driver/x64" |Copy-Item -Destination $zwoDriver
Copy-Item -LiteralPath "$build/licenses" -Destination $stage -Recurse

foreach($name in @('nina-livestack','hocus-focus','erfa','zwo','astap')){
    New-Item -ItemType Directory -Force "$stage/licenses/$name"|Out-Null
    Get-ChildItem -LiteralPath "$root/licenses/$name" | Copy-Item -Destination "$stage/licenses/$name" -Recurse -Force
}
foreach($item in @(@('imgui','external/imgui/LICENSE.txt'),@('glfw','external/glfw/LICENSE.md'),@('json','external/json/LICENSE.MIT'),@('cpp-httplib','external/httplib/LICENSE'),@('IXWebSocket','external/ixwebsocket/LICENSE.txt'),@('QR-Code-generator','external/qrcodegen/cpp/qrcodegen.hpp'))){
    $dest=Join-Path $stage "licenses/$($item[0])"
    New-Item -ItemType Directory -Force $dest | Out-Null
    Copy-Item -LiteralPath (Join-Path $root $item[1]) -Destination $dest
}
# App-local Microsoft runtime from the installed Visual Studio redistributable.
$vswhere="${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$installations=& $vswhere -all -products '*' -property installationPath
$runtime=@(foreach($installation in $installations){
    Get-ChildItem -Path "$installation/VC/Redist/MSVC/*/x64/Microsoft.VC*.CRT" -Directory -ErrorAction SilentlyContinue
}) | Sort-Object { [version]$_.Parent.Parent.Name } -Descending | Select-Object -First 1
if(!$runtime){throw 'Visual C++ x64 redistributable not found'}
foreach($name in @('msvcp140.dll','msvcp140_atomic_wait.dll','vcruntime140.dll','vcruntime140_1.dll')) {
    Copy-Item -LiteralPath (Join-Path $runtime.FullName $name) -Destination $stage
}
# Installing these app-local runtime files is part of the MSI transaction.
# Never depend on a globally installed VC++ runtime on the packaging PC.
$requiredNative=@('ASICamera2.dll','EDSDK.dll','EdsImage.dll','msvcp140.dll','msvcp140_atomic_wait.dll','vcruntime140.dll','vcruntime140_1.dll')
foreach($name in $requiredNative){if(!(Test-Path -LiteralPath (Join-Path $stage $name))){throw "Missing runtime dependency: $name"}}
if(!(Test-Path -LiteralPath "$stage/licenses/EDSDK/readme.txt")){throw 'Canon runtime notices are missing'}
foreach($name in @('LICENSE.txt','NOTICE.md','NinaGeometry.cpp','StackMath.hpp')) {
    if(!(Test-Path -LiteralPath "$stage/licenses/nina-livestack/$name")){throw "NINA LiveStack license or modified source missing: $name"}
}
$nativeFiles=@(Get-Item -LiteralPath "$stage/AstroController.exe")+@(Get-ChildItem -LiteralPath $stage -Filter '*.dll')
$nativeManifest=@(foreach($file in $nativeFiles){
    $stream=[IO.File]::OpenRead($file.FullName);$reader=[IO.BinaryReader]::new($stream)
    try{
        if($reader.ReadUInt16() -ne 0x5a4d){throw "Invalid PE binary: $($file.Name)"}
        $stream.Position=0x3c;$pe=$reader.ReadInt32();$stream.Position=$pe
        if($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664){throw "Expected an x64 PE binary: $($file.Name)"}
    }finally{$reader.Dispose();$stream.Dispose()}
    [ordered]@{file=$file.Name;version=$file.VersionInfo.FileVersion;sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLower()}
})
[ordered]@{application_version=$Version;architecture='x64';deployment='app-local, installed offline by MSI';native_files=$nativeManifest} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath "$stage/dependencies/manifest.json" -Encoding utf8
# Keep mandatory legal notices, but no loose project/source files in the runtime.
$sourceFiles=@(Get-ChildItem -LiteralPath "$stage/licenses" -Recurse -File | Where-Object Extension -in '.cpp','.hpp','.h')
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=[IO.Compression.ZipFile]::Open("$stage/licenses/modified-sources.zip",[IO.Compression.ZipArchiveMode]::Create)
try { foreach($file in $sourceFiles) {
    $entry=[IO.Path]::GetRelativePath("$stage/licenses",$file.FullName).Replace('\','/')
    [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$file.FullName,$entry)|Out-Null
} } finally { $archive.Dispose() }
foreach($file in $sourceFiles){Remove-Item -LiteralPath $file.FullName}
foreach($file in Get-ChildItem -LiteralPath "$stage/licenses" -Recurse -File -Filter '*.md') {
    Rename-Item -LiteralPath $file.FullName -NewName ($file.Name+'.txt')
}
$unexpected=@(Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object Extension -in '.md','.png','.jpg','.cpp','.hpp','.h','.ps1')
if($unexpected.Count){throw 'Development/documentation files leaked into the installer payload'}
$rootFiles=@('AstroController.exe','ASICamera2.dll','EDSDK.dll','EdsImage.dll','msvcp140.dll','msvcp140_atomic_wait.dll','vcruntime140.dll','vcruntime140_1.dll','defaults.json','Cousine-Regular.ttf')
if(@(Get-ChildItem -LiteralPath $stage -File | Where-Object Name -notin $rootFiles).Count){throw 'Unexpected runtime root file'}
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
        $fileId=if($path -eq 'AstroController.exe'){'AppExe'}else{"file_$id"}
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
  <Package InstallerVersion="500" Compressed="yes" InstallPrivileges="elevated" Platform="x64" Description="AstroPhoto Canon and ZWO controller with True North" />
  <MajorUpgrade AllowSameVersionUpgrades="yes" DowngradeErrorMessage="A newer version of AstroPhoto is already installed." />
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
     <Shortcut Id="StartMenuShortcut" Name="AstroPhoto" Target="[INSTALLFOLDER]AstroController.exe" WorkingDirectory="INSTALLFOLDER" Icon="AppIcon" />
     <RegistryValue Root="HKCU" Key="Software\AstroPhoto\Installer" Name="Shortcut" Type="integer" Value="1" KeyPath="yes" />
     <RegistryValue Root="HKCU" Key="Software\AstroPhoto\Installer" Name="InstallFolder" Type="string" Value="[INSTALLFOLDER]" />
     <RemoveFile Id="OldExe" Directory="INSTALLFOLDER" Name="AstroCanonController.exe" On="install" />
     <RemoveFile Id="OldPanels" Directory="INSTALLFOLDER" Name="desktop-panel-*.png" On="install" />
     <RemoveFile Id="OldSmokeImage" Directory="INSTALLFOLDER" Name="workspace-smoke.png" On="install" />
     <RemoveFile Id="OldSmokeReport" Directory="INSTALLFOLDER" Name="desktop-smoke.txt" On="install" />
     <RemoveFile Id="OldNorthSmoke" Directory="INSTALLFOLDER" Name="true-north-smoke.json" On="install" />
     <RemoveFolder Id="RemoveMenu" On="uninstall" />
     <RemoveFolder Id="RemovePrograms" Directory="ProgramsFolder" On="uninstall" />
     $removeDirectories
    </Component>
   </Directory></Directory>
  </Directory>
  <Binary Id="ZwoDriverInstaller" SourceFile="$(Escape-Xml "$build/astro_install_zwo_driver.exe")" />
  <CustomAction Id="InstallZwoUsbDriver" BinaryKey="ZwoDriverInstaller" ExeCommand="&quot;[INSTALLFOLDER]drivers\zwo\ASICAMUSB3.inf&quot;" Execute="deferred" Impersonate="no" Return="check" />
  <InstallExecuteSequence><Custom Action="InstallZwoUsbDriver" After="InstallFiles">NOT REMOVE~=&quot;ALL&quot;</Custom></InstallExecuteSequence>
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
if(Test-Path -LiteralPath $release){
    if((Resolve-Path -LiteralPath $release).Path -ne [IO.Path]::GetFullPath((Join-Path $root 'out/Release'))){throw 'Invalid portable staging path'}
    $preserved=[IO.Path]::GetFullPath((Join-Path $out ('preserved/portable-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfff'))))
    if(!$preserved.StartsWith([IO.Path]::GetFullPath($out)+[IO.Path]::DirectorySeparatorChar)){throw 'Invalid portable backup path'}
    New-Item -ItemType Directory -Force (Split-Path -Parent $preserved)|Out-Null
    Move-Item -LiteralPath $release -Destination $preserved
}
New-Item -ItemType Directory -Force $release|Out-Null
Get-ChildItem -LiteralPath $stage | Copy-Item -Destination $release -Recurse -Force
Get-FileHash -LiteralPath $msi -Algorithm SHA256 | ForEach-Object {"$($_.Hash.ToLower())  $(Split-Path -Leaf $msi)"} | Set-Content -LiteralPath "$msi.sha256" -Encoding ascii
Write-Output "Portable: $release"
Write-Output "Installer: $msi"
