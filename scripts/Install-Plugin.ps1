param(
    [Parameter(Mandatory=$true)][string]$GamePath,
    [Parameter(Mandatory=$true)][string]$PackagePath,
    [Parameter(Mandatory=$true)][string]$PluginId
)
$ErrorActionPreference='Stop'
if(Get-Process -Name LEGOStarWarsSaga -ErrorAction SilentlyContinue){throw 'Close the game before installing a plugin.'}
$gameFolder=(Resolve-Path -LiteralPath $GamePath).Path
$source=(Resolve-Path -LiteralPath $PackagePath).Path
if(!(Test-Path -LiteralPath (Join-Path $gameFolder 'd3d9.dll'))){throw 'Install TCS Mod Loader first.'}
$mods=Join-Path $gameFolder 'Mods'
New-Item -ItemType Directory -Path $mods -Force | Out-Null
if((Get-Item -LiteralPath $mods).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Mods must be an ordinary directory.'}
Add-Type -AssemblyName System.IO.Compression.FileSystem
function Get-PluginId([string]$Path) {
    $archive=[IO.Compression.ZipFile]::OpenRead($Path)
    try {
        $entry=$archive.GetEntry('manifest.ini')
        if($entry){
            $reader=[IO.StreamReader]::new($entry.Open())
            try {$manifest=$reader.ReadToEnd()} finally {$reader.Dispose()}
            if($manifest -match '(?im)^\s*Id\s*=\s*([^\r\n]+)'){return $Matches[1].Trim()}
        }
        return ''
    } finally {$archive.Dispose()}
}
if((Get-PluginId $source) -ne $PluginId){throw 'Source package has the wrong plugin ID.'}
$sourceHash=(Get-FileHash -LiteralPath $source).Hash
$matching=@(Get-ChildItem -LiteralPath $mods -File -Filter '*.zip' | Where-Object {(Get-PluginId $_.FullName) -eq $PluginId} | Sort-Object LastWriteTime -Descending)
$backup=Join-Path $mods 'Backups'
$previous=Join-Path $backup 'PreviousInstall.zip.bak'
$saved=$false
foreach($file in $matching){
    $resolvedFile=(Resolve-Path -LiteralPath $file.FullName).Path
    if([IO.Path]::GetDirectoryName($resolvedFile) -ne (Resolve-Path -LiteralPath $mods).Path -or
       ($file.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Plugin path is outside the ordinary Mods folder.'}
    if((Get-FileHash -LiteralPath $resolvedFile).Hash -ne $sourceHash -and !$saved){
        New-Item -ItemType Directory -Path $backup -Force | Out-Null
        if((Get-Item -LiteralPath $backup).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Backups must be an ordinary directory.'}
        Copy-Item -LiteralPath $resolvedFile -Destination $previous -Force
        if((Get-FileHash -LiteralPath $previous).Hash -ne (Get-FileHash -LiteralPath $resolvedFile).Hash){throw 'Backup verification failed.'}
        $saved=$true
    }
}
$destination=Join-Path $mods ([IO.Path]::GetFileName($source))
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $destination).Hash -ne $sourceHash){throw 'Installed package mismatch.'}
foreach($file in $matching){
    if($file.FullName -ne $destination){Remove-Item -LiteralPath $file.FullName}
}
# Remove only timestamp backups produced by the old installers. Unrelated user
# files are untouched; both plugin installers share one previous-package slot.
if(Test-Path -LiteralPath $backup){
    if((Get-Item -LiteralPath $backup).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Backups must be an ordinary directory.'}
    $legacy=@(Get-ChildItem -LiteralPath $backup -File | Where-Object {$_.Name -match '^TCS(?:FirstPerson|AllyDamage)-.*\.zip\.\d{8}-\d{6}\.bak$'} | Sort-Object LastWriteTime -Descending)
    if(!(Test-Path -LiteralPath $previous) -and $legacy.Count){Copy-Item -LiteralPath $legacy[0].FullName -Destination $previous}
    foreach($file in $legacy){Remove-Item -LiteralPath $file.FullName}
}
Write-Output "Installed $destination"
Write-Output 'Open Manage Mod Priorities.cmd and choose Save & prepare or Launch game.'
