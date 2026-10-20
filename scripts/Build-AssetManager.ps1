param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
& $Python -m PyInstaller --noconfirm --clean --onefile --name TCSAssetManager --distpath (Join-Path $repo 'build/Release') --workpath (Join-Path $repo 'build/asset-helper') --specpath (Join-Path $repo 'build') (Join-Path $repo 'tools/asset_manager.py')
if ($LASTEXITCODE -ne 0) { throw 'Asset helper build failed. The developer build needs Python and PyInstaller; the released helper does not.' }
