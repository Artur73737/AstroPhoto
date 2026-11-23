$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$files=Get-ChildItem -LiteralPath "$root/src" -Recurse -File -Include *.cpp,*.hpp
$violations=@()
foreach($file in $files){
 $relative=[IO.Path]::GetRelativePath($root,$file.FullName).Replace('\','/')
 if($relative -notlike 'src/cameras/canon/edsdk/*'){$violations+=@($file|Select-String '#\s*include\s*[<"]EDSDK')}
 if($relative -notlike 'src/cameras/zwo/sdk/*'){$violations+=@($file|Select-String '#\s*include\s*[<"].*ASICamera2')}
 if($relative -like 'src/core/*'){$violations+=@($file|Select-String '#\s*include\s*[<"](imgui|windows|GLFW|astro/cameras)')}
}
if($violations.Count){$violations|Write-Output;throw 'Forbidden module dependency'}
'Boundaries verified: pure core, isolated Canon and ASI SDK headers.'
