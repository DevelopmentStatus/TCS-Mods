param([Parameter(Mandatory=$true)][string]$GamePath)
& (Join-Path $PSScriptRoot 'scripts\Install-Plugin.ps1') -GamePath $GamePath -PackagePath (Join-Path $PSScriptRoot 'dist\TCSAllyDamage-v1.1.1.zip') -PluginId 'tcs.allydamage'
