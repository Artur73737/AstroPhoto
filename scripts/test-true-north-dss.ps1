param([switch]$Download,[string]$Astap='external/astap/astap.exe',[string]$Catalog='external/astap')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$directory=Join-Path $root 'out/true-north/dss'
New-Item -ItemType Directory -Force $directory |Out-Null
function Assert-Fits([string]$path){$stream=[IO.File]::OpenRead($path);try{$bytes=New-Object byte[] 6;if($stream.Read($bytes,0,6) -ne 6 -or [Text.Encoding]::ASCII.GetString($bytes) -ne 'SIMPLE'){throw "Not a FITS image: $path"}}finally{$stream.Dispose()}}
if($Download){
 foreach($n in 1,2){$size=$n*60;$file=Join-Path $directory "pole-offset-$n.fits";if(!(Test-Path -LiteralPath $file)){Invoke-WebRequest "https://archive.eso.org/dss/dss/image?ra=02%2040%2000&dec=%2B88%2042%2000&equinox=J2000&x=$size&y=$size&Sky-Survey=DSS2-red&mime-type=download-fits" -OutFile $file};Assert-Fits $file}
 # Four genuine DSS2 tiles reprojected by C++; RA decimal degrees, not hours.
 $centres=@(@(19.90202166967368,87.81724398429314),@(60.09797833032632,87.81724398429314),@(346.257886574006,89.07001653198196),@(93.74211342599402,89.07001653198196))
 for($i=0;$i -lt 4;$i++){$ra=$centres[$i][0].ToString('R',[Globalization.CultureInfo]::InvariantCulture);$dec=$centres[$i][1].ToString('R',[Globalization.CultureInfo]::InvariantCulture);$file=Join-Path $directory "correct-tile-$i.fits";if(!(Test-Path -LiteralPath $file)){Invoke-WebRequest "https://archive.eso.org/dss/dss/image?ra=$ra&dec=$dec&equinox=J2000&x=120&y=120&Sky-Survey=DSS2-red&mime-type=download-fits" -OutFile $file};Assert-Fits $file}
 & "$root/out/build/Release/astro_dss_mosaic.exe" "$directory/pole-offset-3.fits" "$directory/correct-tile-0.fits" "$directory/correct-tile-1.fits" "$directory/correct-tile-2.fits" "$directory/correct-tile-3.fits"
 if($LASTEXITCODE -ne 0){throw 'DSS mosaic failed or coverage below 99 percent'}
}
foreach($n in 1,2,3){Assert-Fits (Join-Path $directory "pole-offset-$n.fits")}
$env:ASTRO_NORTH_DSS_DIRECTORY=$directory
$env:ASTRO_ASTAP_EXE=(Resolve-Path (Join-Path $root $Astap)).Path
$env:ASTRO_ASTAP_DATABASE=(Resolve-Path (Join-Path $root $Catalog)).Path
try{& "$root/out/build/Release/astro_north_dss_tests.exe";if($LASTEXITCODE -ne 0){throw 'Real DSS/ASTAP True North tests failed'}}finally{Remove-Item Env:ASTRO_NORTH_DSS_DIRECTORY,Env:ASTRO_ASTAP_EXE,Env:ASTRO_ASTAP_DATABASE -ErrorAction SilentlyContinue}
