[CmdletBinding()]
param(
    [string]$FO4EditDir = "C:\Games\Modding\FO4Tools\FO4Edit 4.1.5f",
    [string]$GameDataDir = "C:\games\steam\SteamApps\common\Fallout 4\Data"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$scriptName = "HR_BuildHouseRulesESP.pas"
$scriptSrc = Join-Path $PSScriptRoot $scriptName
$scriptDst = Join-Path $FO4EditDir "Edit Scripts\$scriptName"
$exe = Join-Path $FO4EditDir "FO4Edit.exe"
$gameEsp = Join-Path $GameDataDir "HouseRules.esp"
$repoEsp = Join-Path $repoRoot "package\core\HouseRules.esp"

if (-not (Test-Path $exe)) { throw "FO4Edit.exe not found at $exe" }
if (-not (Test-Path $scriptSrc)) { throw "Script not found at $scriptSrc" }

Copy-Item $scriptSrc $scriptDst -Force
Remove-Item (Join-Path $FO4EditDir "HR_BuildProgress.log") -ErrorAction SilentlyContinue
Remove-Item (Join-Path $FO4EditDir "HR_LastScriptDone.txt") -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "Copied $scriptName to FO4Edit."
Write-Host ""
Write-Host "Manual FO4Edit steps:"
Write-Host "1. In Module Selection, select only Fallout4.esm, then click OK."
Write-Host "2. After loading finishes, right-click Fallout4.esm, choose Apply Script, then select HR_BuildHouseRulesESP."
Write-Host "3. Wait for HR_BuildProgress.log to end with: step 4 done -- sentinel written"
Write-Host "4. Close the FO4Script result window, then close FO4Edit."
Write-Host "5. In the save prompt, check HouseRules.esp and save it."
Write-Host ""

Start-Process -FilePath $exe -ArgumentList @("-fo4") -WorkingDirectory $FO4EditDir

Read-Host "Press Enter here after FO4Edit has saved HouseRules.esp"

if (-not (Test-Path $gameEsp)) {
    throw "HouseRules.esp was not found at $gameEsp"
}

Copy-Item $gameEsp $repoEsp -Force
Write-Host "Copied $gameEsp to $repoEsp"

Push-Location $repoRoot
try {
    xmake build
} finally {
    Pop-Location
}
