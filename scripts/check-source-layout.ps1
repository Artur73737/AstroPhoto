$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$errors=@()
foreach($file in Get-ChildItem -LiteralPath "$root/src" -Recurse -File){
 $path=[IO.Path]::GetRelativePath($root,$file.FullName).Replace('\','/')
 if($file.Extension -eq '.hpp' -and $path -notmatch '/include/astro/'){$errors+="Public header outside feature include tree: $path"}
 if($file.Extension -eq '.cpp' -and $path -notmatch '/(src|tests)/'){$errors+="Implementation outside feature source/test tree: $path"}
 if($file.Extension -in '.cpp','.hpp'){
  foreach($match in $file|Select-String '#\s*include\s*"([^"/]+\.hpp)"'){$errors+="Adjacent header include: $path : $($match.Line)"}
 }
}
if($errors.Count){$errors;throw 'Source layout check failed'}
'Feature source/include layout verified; independent public-header compile checks are built by CMake.'
