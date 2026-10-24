param(
    [string]$BuildPath = (Join-Path $PSScriptRoot '..\build\Release'),
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\build\packages')
)
$ErrorActionPreference = 'Stop'
$repoPath = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$components = @(
    @{Name='TCSModLoader'; Version='1.6.6'; Dll='d3d9.dll'},
    @{Name='TCSFirstPerson'; Version='0.4.0'; Dll='TCSFirstPerson.dll'},
    @{Name='TCSModernOptions'; Version='0.4.0'; Dll='TCSModernOptions.dll'},
    @{Name='TCSAllyDamage'; Version='1.1.1'; Dll='TCSAllyDamage.dll'},
    @{Name='TCSSkipSplash'; Version='1.0.0'; Dll='TCSSkipSplash.dll'}
    @{Name='TCSShopOwnershipFix'; Version='1.0.0'; Dll='TCSShopOwnershipFix.dll'}
)
foreach ($component in $components) {
    if (!(Test-Path -LiteralPath (Join-Path $BuildPath $component.Dll))) {
        throw "Missing $($component.Dll). Build Win32 Release before packaging."
    }
}
New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
if (!(Test-Path -LiteralPath (Join-Path $BuildPath 'TCSAssetManager.exe'))) {
    throw 'Missing TCSAssetManager.exe. Run scripts/Build-AssetManager.ps1 before packaging.'
}
# Unique staging folders avoid picking up files from previous packaging runs.
$stageRoot = Join-Path $OutputPath ('stage-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stageRoot | Out-Null
foreach ($component in $components) {
    $stage = Join-Path $stageRoot $component.Name
    New-Item -ItemType Directory -Path $stage | Out-Null
    Copy-Item -Path (Join-Path $repoPath "packages\$($component.Name)\*") -Destination $stage -Recurse
    Copy-Item -LiteralPath (Join-Path $BuildPath $component.Dll) -Destination $stage
    if ($component.Name -eq 'TCSModLoader') {
        Copy-Item -LiteralPath (Join-Path $BuildPath 'TCSAssetManager.exe') -Destination $stage
        $docsRoot = Join-Path $repoPath 'docs'
        foreach ($doc in Get-ChildItem -LiteralPath $docsRoot -File -Recurse) {
            $relativeDoc = $doc.FullName.Substring($docsRoot.Length + 1)
            if ($relativeDoc.StartsWith('research\', [StringComparison]::OrdinalIgnoreCase) -and
                $doc.Extension -eq '.md' -and $doc.Name -ne 'asset-overrides.md') {
                continue
            }
            $docTarget = Join-Path (Join-Path $stage 'docs') $relativeDoc
            New-Item -ItemType Directory -Path (Split-Path -Parent $docTarget) -Force | Out-Null
            Copy-Item -LiteralPath $doc.FullName -Destination $docTarget
        }
        $mods = Join-Path $stage 'Mods'
        New-Item -ItemType Directory -Path $mods | Out-Null
        Set-Content -LiteralPath (Join-Path $mods 'README.txt') -Value 'Place plugin ZIPs or supported asset ZIP/RAR archives directly here. Keep them compressed. Remove an archive and restart to restore underlying assets. See docs/ASSET_IMPORT.md.' -Encoding UTF8
    }
    $zip = Join-Path $OutputPath "$($component.Name)-v$($component.Version).zip"
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -Force
    Write-Output $zip
}
# Staging folders stay beside the output for inspection; all are ignored by Git.
