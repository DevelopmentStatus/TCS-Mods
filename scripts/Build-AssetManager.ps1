param(
    [string]$Python = 'python',
    [string]$BuildPath = (Join-Path $PSScriptRoot '..\build\Release'),
    [string]$WorkPath = (Join-Path $PSScriptRoot '..\build\asset-helper-onedir')
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
# PyInstaller writes progress to stderr. Windows PowerShell 5 treats redirected
# progress as NativeCommandError records, so use the actual native exit code.
$helperErrorPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = 'Continue'
    & $Python -m PyInstaller --noconfirm --clean --onedir --noupx --contents-directory TCSAssetRuntime --name TCSAssetManager --distpath $BuildPath --workpath $WorkPath --specpath $WorkPath (Join-Path $repo 'tools/asset_manager.py')
    $helperExitCode = $LASTEXITCODE
} finally {
    $ErrorActionPreference = $helperErrorPreference
}
if ($helperExitCode -ne 0) { throw 'Asset helper build failed. The developer build needs Python and PyInstaller; the released helper does not.' }
