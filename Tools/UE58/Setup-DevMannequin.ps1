# DARK BLOOD - development character setup (UE 5.8).
#
# Copies the engine's third-person template mannequin (Manny/Quinn, locomotion AnimBP, attack/dash/hit/death
# animations) into this project and builds the DEV montages used by the visual profiles.
# The copied content comes from YOUR local Unreal Engine installation and is NOT committed (see .gitignore):
# every developer runs this script once. Without it the game shows the greybox cylinder bodies.
#
# Usage:  powershell -ExecutionPolicy Bypass -File Tools\UE58\Setup-DevMannequin.ps1 [-EngineDir D:\UE_5.8]

param(
	[string]$EngineDir = ""
)

$ErrorActionPreference = "Stop"
$ProjectDir = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$Project = Join-Path $ProjectDir "DarkBlood.uproject"

if ($EngineDir -eq "")
{
	$Key = "HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.8"
	if (Test-Path $Key) { $EngineDir = (Get-ItemProperty $Key).InstalledDirectory }
}
if ($EngineDir -eq "" -or -not (Test-Path $EngineDir))
{
	throw "Unreal Engine 5.8 not found. Pass -EngineDir <path to UE_5.8>."
}

$Source = Join-Path $EngineDir "Templates\TemplateResources\High\Characters\Content\Mannequins"
$Target = Join-Path $ProjectDir "Content\Characters\Mannequins"
if (-not (Test-Path $Source)) { throw "Template mannequin not found at $Source" }

Write-Host "Copying template mannequin -> $Target"
robocopy $Source $Target /E /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE)" }

$Editor = Join-Path $EngineDir "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Script = Join-Path $PSScriptRoot "db_setup_dev_animations.py"
Write-Host "Building DEV montages ..."
& $Editor $Project -run=pythonscript "-script=$Script" -unattended -nosplash -nullrhi -stdout -FullStdOutLogOutput | Select-String -Pattern "DBSETUP|Error" | ForEach-Object { $_.Line }
Write-Host "Done. Start the game: characters now use the mannequin instead of the greybox cylinder."
