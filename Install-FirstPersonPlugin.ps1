param([Parameter(Mandatory=$true)][string]$GamePath)
& (Join-Path $PSScriptRoot 'scripts\Install-Plugin.ps1') -GamePath $GamePath -PackagePath (Join-Path $PSScriptRoot 'dist\TCSFirstPerson-v0.4.0.zip') -PluginId 'tcs.firstperson'
