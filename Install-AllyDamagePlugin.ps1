param([Parameter(Mandatory=$true)][string]$GamePath)
$ErrorActionPreference='Stop'
if(Get-Process -Name LEGOStarWarsSaga -ErrorAction SilentlyContinue){throw 'Close the game before installing TCS Ally Damage.'}
$gameFolder=(Resolve-Path -LiteralPath $GamePath).Path
$mods=Join-Path $gameFolder 'Mods'
$source=Join-Path $PSScriptRoot 'dist\TCSAllyDamage-v1.1.1.zip'
if(!(Test-Path -LiteralPath $source)){throw 'Ally Damage package missing.'}
if(!(Test-Path -LiteralPath (Join-Path $gameFolder 'd3d9.dll'))){throw 'Install TCS Mod Loader first.'}
New-Item -ItemType Directory -Path $mods -Force | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
foreach($file in (Get-ChildItem -LiteralPath $mods -File -Filter '*.zip')) {
    $archive=[System.IO.Compression.ZipFile]::OpenRead($file.FullName)
    try {
        $entry=$archive.GetEntry('manifest.ini');$id=''
        if($entry) {
            $reader=[System.IO.StreamReader]::new($entry.Open())
            try {$manifest=$reader.ReadToEnd()} finally {$reader.Dispose()}
            if($manifest -match '(?im)^\s*Id\s*=\s*([^\r\n]+)'){$id=$Matches[1].Trim()}
        }
    } finally {$archive.Dispose()}
    if($id -eq 'tcs.allydamage') {
        $resolvedFile=(Resolve-Path -LiteralPath $file.FullName).Path
        $resolvedMods=(Resolve-Path -LiteralPath $mods).Path
        if([System.IO.Path]::GetDirectoryName($resolvedFile) -ne $resolvedMods){throw 'Package path is outside the game Mods folder.'}
        $backup=Join-Path $mods 'Backups'
        New-Item -ItemType Directory -Path $backup -Force | Out-Null
        Move-Item -LiteralPath $resolvedFile -Destination (Join-Path $backup ($file.Name+'.'+$stamp+'.bak'))
    }
}
$destination=Join-Path $mods 'TCSAllyDamage-v1.1.1.zip'
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $destination).Hash){throw 'Installed package mismatch.'}
Write-Output "Installed $destination"
Write-Output 'Start the game. Open Pause > Mod Settings > TCS Ally Damage > Active combat AI.'
