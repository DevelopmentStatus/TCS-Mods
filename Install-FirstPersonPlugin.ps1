param([Parameter(Mandatory=$true)][string]$GamePath)
$ErrorActionPreference='Stop'
if(Get-Process -Name LEGOStarWarsSaga -ErrorAction SilentlyContinue){ Write-Output 'Package ready; close the game and run Install-FirstPersonPlugin.ps1 to install it.'; exit 0 }
$gameFolder=(Resolve-Path -LiteralPath $GamePath).Path
$mods=Join-Path $gameFolder 'Mods'
$source=Join-Path $PSScriptRoot 'dist\TCSFirstPerson-v0.4.0.zip'
if(!(Test-Path -LiteralPath $source)){throw 'Plugin package missing.'}
if(!(Test-Path -LiteralPath (Join-Path $gameFolder 'd3d9.dll'))){throw 'Install TCS ModLoader before installing this plugin.'}
New-Item -ItemType Directory -Path $mods -Force | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
foreach($file in (Get-ChildItem -LiteralPath $mods -File -Filter '*.zip')){
    $archive=[System.IO.Compression.ZipFile]::OpenRead($file.FullName)
    try {
        $entry=$archive.GetEntry('manifest.ini')
        $id=''
        if($entry){
            $reader=[System.IO.StreamReader]::new($entry.Open())
            try {$manifest=$reader.ReadToEnd()} finally {$reader.Dispose()}
            if($manifest -match '(?im)^\s*Id\s*=\s*([^\r\n]+)'){ $id=$Matches[1].Trim() }
        }
    } finally {$archive.Dispose()}
    if($id -eq 'tcs.firstperson'){
        $backup=Join-Path $mods 'Backups'
        New-Item -ItemType Directory -Path $backup -Force | Out-Null
        Move-Item -LiteralPath $file.FullName -Destination (Join-Path $backup ($file.Name+'.'+$stamp+'.bak'))
    }
}
$destination=Join-Path $mods 'TCSFirstPerson-v0.4.0.zip'
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $destination).Hash){throw 'Installed package mismatch.'}
Write-Output "Installed $destination"
Write-Output 'Open Pause > Mod Settings > TCS First Person after starting the game.'
