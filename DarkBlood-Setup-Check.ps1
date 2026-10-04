# DARK BLOOD - Pruefung und Einrichtung der Entwicklungsumgebung nach dem Windows-Reset
#
# Nur pruefen (aendert nichts):
#   powershell -ExecutionPolicy Bypass -File DarkBlood-Setup-Check.ps1
# Fehlendes installieren (PowerShell "Als Administrator ausfuehren"):
#   powershell -ExecutionPolicy Bypass -File DarkBlood-Setup-Check.ps1 -Install
# Zusaetzlich Blender und Epic Games Launcher installieren:
#   ... -Install -Optional
# Danach DarkBloodEditor bauen (Win64 Development):
#   ... -Build
#
# Das Skript formatiert, loescht oder verschiebt keine Laufwerke und keine Projektdateien.

param(
	[string]$ProjectDir = "C:\Projekte\dark-Blood",
	[string]$EngineDir = "D:\UE_5.8",
	[switch]$Install,
	[switch]$Optional,
	[switch]$Build
)

$ErrorActionPreference = "Continue"
$Results = New-Object System.Collections.Generic.List[object]

function Report([string]$Name, [string]$State, [string]$Detail) {
	$Results.Add([pscustomobject]@{ Pruefung = $Name; Status = $State; Detail = $Detail })
	$Color = @{ OK = "Green"; FEHLT = "Red"; WARNUNG = "Yellow"; INFO = "Gray" }[$State]
	Write-Host ("[{0,-7}] {1}: {2}" -f $State, $Name, $Detail) -ForegroundColor $Color
}

function Has([string]$Cmd) { [bool](Get-Command $Cmd -ErrorAction SilentlyContinue) }

function Refresh-Path {
	$env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User")
}

function Winget-Install([string]$Id, [string]$Extra = "") {
	if (-not (Has "winget")) { Write-Host "  winget fehlt - $Id bitte von Hand installieren." -ForegroundColor Red; return }
	Write-Host "  Installiere $Id ..." -ForegroundColor Cyan
	$WgArgs = @("install", "--id", $Id, "-e", "--accept-package-agreements", "--accept-source-agreements")
	if ($Extra -ne "") { $WgArgs += @("--override", $Extra) }
	& winget @WgArgs
	Refresh-Path
}

$IsAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if ($Install -and -not $IsAdmin) {
	Write-Host "Fuer -Install bitte PowerShell als Administrator starten." -ForegroundColor Red
	exit 1
}

Write-Host "`n=== Werkzeuge ===" -ForegroundColor Cyan

# winget
if (Has "winget") { Report "winget" "OK" ((winget --version) -join "") }
else { Report "winget" "FEHLT" "App 'App-Installer' aus dem Microsoft Store installieren" }

# Git
if (-not (Has "git") -and $Install) { Winget-Install "Git.Git" }
if (Has "git") { Report "Git" "OK" ((git --version) -join "") } else { Report "Git" "FEHLT" "winget install Git.Git" }

# Git LFS
$LfsOk = $false
if (Has "git") { git lfs version *> $null; $LfsOk = ($LASTEXITCODE -eq 0) }
if (-not $LfsOk -and $Install) { Winget-Install "GitHub.GitLFS"; git lfs version *> $null; $LfsOk = ($LASTEXITCODE -eq 0) }
if ($LfsOk) {
	if ($Install) { git lfs install *> $null }
	Report "Git LFS" "OK" ((git lfs version) -join "")
} else { Report "Git LFS" "FEHLT" "winget install GitHub.GitLFS" }

# Visual Studio 2022 mit C++ (MSVC v143) und Windows SDK
$VsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$VsAdd = "--add Microsoft.VisualStudio.Workload.NativeGame --add Microsoft.VisualStudio.Workload.NativeDesktop --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.22621 --includeRecommended"
function Find-VS {
	if (-not (Test-Path $VsWhere)) { return $null }
	& $VsWhere -latest -products * -version "[17.0,18.0)" -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
$VsPath = Find-VS
if (-not $VsPath -and $Install) {
	$AnyVs = if (Test-Path $VsWhere) { & $VsWhere -latest -products * -version "[17.0,18.0)" -property installationPath } else { $null }
	if ($AnyVs) {
		Write-Host "  Ergaenze C++-Komponenten in $AnyVs ..." -ForegroundColor Cyan
		$Setup = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\setup.exe"
		Start-Process $Setup -ArgumentList ("modify --installPath `"$AnyVs`" $VsAdd --passive --norestart") -Wait
	} else {
		Winget-Install "Microsoft.VisualStudio.2022.Community" "--passive --wait --norestart $VsAdd"
	}
	$VsPath = Find-VS
}
if ($VsPath) { Report "Visual Studio 2022 C++" "OK" $VsPath }
else { Report "Visual Studio 2022 C++" "FEHLT" "Workloads 'Spieleentwicklung mit C++' und 'Desktopentwicklung mit C++'" }

$SdkInc = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\Include"
$Sdks = if (Test-Path $SdkInc) { Get-ChildItem $SdkInc -Directory | Where-Object Name -like "10.*" | Sort-Object Name } else { @() }
if ($Sdks.Count -gt 0) { Report "Windows SDK" "OK" (($Sdks | Select-Object -ExpandProperty Name) -join ", ") }
else { Report "Windows SDK" "FEHLT" "kommt mit den Visual-Studio-Workloads" }

# CMake (nur Regelkern-Tests)
if (-not (Has "cmake") -and $Install) { Winget-Install "Kitware.CMake" }
if (Has "cmake") { Report "CMake" "OK" ((cmake --version | Select-Object -First 1) -join "") }
else { Report "CMake" "FEHLT" "winget install Kitware.CMake (nur fuer Tests/RulesTests)" }

# Python 3 (Tools/*.py ausserhalb von Unreal)
$Py = $null
foreach ($c in @("py", "python")) {
	if (Has $c) { $v = (& $c --version 2>&1) -join ""; if ($v -match "Python 3") { $Py = $v; break } }
}
if (-not $Py -and $Install) { Winget-Install "Python.Python.3.13"; if (Has "py") { $Py = (py --version 2>&1) -join "" } }
if ($Py) { Report "Python" "OK" $Py } else { Report "Python" "FEHLT" "winget install Python.Python.3.13" }

# Optional: Blender (Tools/prepare_alpha_cards.py erwartet Blender 5.2), Epic Games Launcher
$Blender = Get-ChildItem "C:\Program Files\Blender Foundation" -Directory -ErrorAction SilentlyContinue | Sort-Object Name | Select-Object -Last 1
if (-not $Blender -and $Install -and $Optional) { Winget-Install "BlenderFoundation.Blender"; $Blender = Get-ChildItem "C:\Program Files\Blender Foundation" -Directory -ErrorAction SilentlyContinue | Select-Object -Last 1 }
if ($Blender) {
	$State = if ($Blender.Name -eq "Blender 5.2") { "OK" } else { "WARNUNG" }
	Report "Blender (optional)" $State "$($Blender.Name) - Tools/prepare_alpha_cards.py erwartet 'Blender 5.2'"
} else { Report "Blender (optional)" "INFO" "nicht installiert - nur fuer Asset-Werkzeuge noetig" }

$Launcher = Test-Path "${env:ProgramFiles(x86)}\Epic Games\Launcher\Portal\Binaries\Win64\EpicGamesLauncher.exe"
if (-not $Launcher -and $Install -and $Optional) { Winget-Install "EpicGames.EpicGamesLauncher"; $Launcher = Test-Path "${env:ProgramFiles(x86)}\Epic Games\Launcher\Portal\Binaries\Win64\EpicGamesLauncher.exe" }
if ($Launcher) { Report "Epic Games Launcher (optional)" "OK" "installiert" }
else { Report "Epic Games Launcher (optional)" "INFO" "nicht installiert - fuer Doppelklick auf .uproject und Engine-Updates" }

Write-Host "`n=== Unreal Engine 5.8 ===" -ForegroundColor Cyan

$Editor = Join-Path $EngineDir "Engine\Binaries\Win64\UnrealEditor.exe"
$BuildBat = Join-Path $EngineDir "Engine\Build\BatchFiles\Build.bat"
if (Test-Path $Editor) {
	$Ver = (Get-Item $Editor).VersionInfo.ProductVersion
	Report "UnrealEditor.exe" "OK" "$Editor ($Ver)"
} else { Report "UnrealEditor.exe" "FEHLT" "nicht unter $EngineDir - Pfad mit -EngineDir angeben" }
if (Test-Path $BuildBat) { Report "Build.bat" "OK" $BuildBat } else { Report "Build.bat" "FEHLT" $BuildBat }

$Reg = "HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.8"
if (Test-Path $Reg) { Report "Engine-Registrierung" "OK" (Get-ItemProperty $Reg).InstalledDirectory }
else { Report "Engine-Registrierung" "WARNUNG" "fehlt (normal nach Reset). Bauen/Starten geht trotzdem per Pfad; Setup-DevMannequin.ps1 mit -EngineDir $EngineDir aufrufen" }

$Prereq = Join-Path $EngineDir "Engine\Extras\Redist\en-us\UEPrereqSetup_x64.exe"
if (Test-Path $Prereq) {
	if ($Install) { Write-Host "  Installiere Unreal-Laufzeitvoraussetzungen ..." -ForegroundColor Cyan; Start-Process $Prereq -ArgumentList "/quiet /norestart" -Wait }
	Report "UE-Prerequisites" "INFO" ($(if ($Install) { "ausgefuehrt" } else { "mit -Install ausfuehren (VC++-Runtime, DirectX)" }))
}

Write-Host "`n=== Projekt ===" -ForegroundColor Cyan

if (-not (Test-Path (Join-Path $ProjectDir "DarkBlood.uproject"))) {
	Report "DarkBlood.uproject" "FEHLT" "nicht in $ProjectDir"
} elseif (Has "git") {
	Report "DarkBlood.uproject" "OK" $ProjectDir
	$Out = (git -C $ProjectDir status --porcelain 2>&1) -join "`n"
	if ($Out -match "dubious ownership") {
		if ($Install) {
			git config --global --add safe.directory ($ProjectDir -replace "\\", "/")
			Report "Git-Besitzer" "OK" "safe.directory gesetzt (Ordner stammt aus dem Backup)"
			$Out = (git -C $ProjectDir status --porcelain 2>&1) -join "`n"
		} else { Report "Git-Besitzer" "WARNUNG" "'dubious ownership' - mit -Install wird safe.directory gesetzt" }
	}
	$Branch = (git -C $ProjectDir branch --show-current) -join ""
	$Head = (git -C $ProjectDir log -1 --format="%h %s") -join ""
	Report "Branch" ($(if ($Branch -eq "claude/youthful-gates-fm5r1c") { "OK" } else { "WARNUNG" })) $Branch
	Report "Letzter Commit" ($(if ($Head -like "d1d57ec*") { "OK" } else { "WARNUNG" })) $Head
	$Changed = @($Out -split "`n" | Where-Object { $_ -ne "" -and $_ -notmatch "dubious|safe.directory" })
	if ($Changed.Count -eq 0) { Report "Arbeitsverzeichnis" "OK" "keine lokalen Aenderungen" }
	else { Report "Arbeitsverzeichnis" "WARNUNG" ("{0} geaenderte/neue Dateien, z. B. {1}" -f $Changed.Count, (($Changed | Select-Object -First 5) -join " | ")) }
	git -C $ProjectDir fetch origin 2>&1 | Out-Null
	$Sb = (git -C $ProjectDir status -sb 2>&1 | Select-Object -First 1) -join ""
	Report "Abgleich mit GitHub" ($(if ($Sb -match "ahead|behind") { "WARNUNG" } else { "OK" })) $Sb
	if ($LfsOk) {
		$Pointers = @(git -C $ProjectDir lfs ls-files 2>$null | Where-Object { $_ -match " - " }).Count
		Report "LFS-Platzhalter ohne Inhalt" ($(if ($Pointers -eq 0) { "OK" } else { "FEHLT" })) ($(if ($Pointers -eq 0) { "alle LFS-Dateien haben Inhalt" } else { "$Pointers Dateien - 'git lfs pull' ausfuehren" }))
	}
}

foreach ($f in @("AGENTS.md", "docs\PROJECT_IDENTITY.md")) {
	$p = Join-Path $ProjectDir $f
	if (Test-Path $p) {
		$Tracked = (git -C $ProjectDir ls-files -- $f 2>$null) -join ""
		Report $f ($(if ($Tracked) { "OK" } else { "WARNUNG" })) ($(if ($Tracked) { "vorhanden und in Git" } else { "vorhanden, aber NICHT committet" }))
	} else { Report $f "WARNUNG" "nicht vorhanden" }
}

foreach ($d in @("SourceArt", "Content\DarkBlood\Dev\FabAnims", "Content\DarkBlood\Dev\FabWeapons", "Content\DarkBlood\Dev\Mannequin", "Content\Characters\Mannequins")) {
	$p = Join-Path $ProjectDir $d
	if (Test-Path $p) {
		$n = @(Get-ChildItem $p -Recurse -File -ErrorAction SilentlyContinue).Count
		Report "Nur lokal: $d" "OK" "$n Dateien (nicht in Git - nur hier und im Backup E:)"
	} else { Report "Nur lokal: $d" "WARNUNG" "fehlt - aus E:\DarkBlood-Backup_2026-10-04 holen oder per Tools-Skript neu erzeugen" }
}

if ($Build) {
	Write-Host "`n=== Build DarkBloodEditor Win64 Development ===" -ForegroundColor Cyan
	if ((Test-Path $BuildBat) -and $VsPath) {
		& $BuildBat DarkBloodEditor Win64 Development "-Project=$ProjectDir\DarkBlood.uproject" -WaitMutex -NoHotReloadFromIDE
		if ($LASTEXITCODE -eq 0) { Report "Build" "OK" "DarkBloodEditor gebaut" } else { Report "Build" "FEHLT" "Exit-Code $LASTEXITCODE - Ausgabe oben" }
	} else { Report "Build" "FEHLT" "Build.bat oder Visual Studio fehlt" }
}

$Log = Join-Path $ProjectDir "Saved\DarkBlood-Setup-Check.txt"
New-Item -ItemType Directory -Force -Path (Split-Path $Log) | Out-Null
$Results | Format-Table -AutoSize -Wrap | Out-String -Width 220 | Set-Content -Encoding UTF8 $Log
$Missing = @($Results | Where-Object Status -eq "FEHLT").Count
Write-Host ("`nFertig: {0} fehlt, {1} Warnungen. Bericht: {2}" -f $Missing, @($Results | Where-Object Status -eq "WARNUNG").Count, $Log) -ForegroundColor Cyan
if ($Missing -gt 0 -and -not $Install) { Write-Host "Zum Installieren: PowerShell als Administrator, dann mit -Install erneut starten." -ForegroundColor Yellow }
if (-not $Build) { Write-Host "Zum Bauen danach: mit -Build erneut starten. Editor starten: & '$Editor' '$ProjectDir\DarkBlood.uproject'" -ForegroundColor Yellow }
