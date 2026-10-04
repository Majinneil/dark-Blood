# DARK BLOOD — Projektprüfung vom 29.09.2026

## Arbeitsbasis und Sicherung

- Ausschließlich `C:\Projekte\dark-Blood\DarkBlood.uproject` ist das Zielprojekt.
- Engine: lokal geprüftes Unreal Engine **5.8.3**, Changelist **58210709**, unter `D:\UE_5.8`.
- Ausgangsbranch: `claude/youthful-gates-fm5r1c`.
- Ausgangscommit: `8c81c0d2b0fcd7e3019d9debd100bfd9b88711dd`.
- Sicherheitsbranch: `backup/minecraft-mistake-before-cleanup`, zeigt auf diesen Commit.
- Vor der Änderung waren Arbeitsverzeichnis und Index sauber (`git diff` und `git diff --cached`: 0 Bytes).
- Die Sicherung bewahrt den gesamten versionierten Ausgangsstand. Ignorierte Quelldownloads,
  Build-Ergebnisse und Spielstände sind nicht Teil eines Git-Branches und wurden nicht bearbeitet.

## MINECRAFT-FEHLÄNDERUNGEN

**Keine gefunden.** Inventar vor den Dokumentationsergänzungen: 1.371 versionierte bzw.
nicht ignorierte Dateien, 286 geprüfte Textdateien, 1.037 `.uasset`-/`.umap`-Dateien.
Geprüft wurden Status, Arbeits-/Index-Diffs, die jüngsten acht Commit-Beschreibungen,
Dateiänderungslisten der jüngsten sechs Commits sowie Dateinamen und einschlägige Textinhalte.

Keine Gradle-Modstruktur, `src/main/java`, Minecraft-Pakete, NeoForge-Abhängigkeiten,
`mods.toml` oder `fabric.mod.json` gefunden. Die zusätzliche Dateinamenssuche über die
Projektwurzel einschließlich ignorierter Unterordner fand ebenfalls keine entsprechenden
Mod-Builddateien. Binäre Unreal-Assets wurden inventarisiert, nicht als Text interpretiert.

Der einzige einschlägige Texttreffer vor der Ergänzung dieses Berichts war
`docs/VisualPack/01_CLAUDE_MASTER_TASK.md:11`: eine ausdrückliche Abgrenzung der Grafik
von Minecraft. Das ist keine Fehländerung. `Forge` bezeichnet im C++-Crafting eine Schmiede;
`Fabric` bezeichnet Stoffmaterialien. Beides bleibt erhalten.

Der zuvor bei der Pfadsuche gelesene Ordner `C:\Users\neilh\Documents\DarkBlood` ist ein
separates altes Java-Projekt. Er ist keine Grundlage für diese Unreal-Arbeit. In dieser Sitzung
wurde dort weder etwas implementiert noch gelöscht. Vor dieser Korrektur hatte diese Sitzung
auch im Unreal-Repository keine Dateien geändert.

## KORREKTE UNREAL-DATEIEN / BEIBEHALTEN

- `DarkBlood.uproject`: EngineAssociation `5.8`; Runtime-Module `DarkBloodRules` und `DarkBlood`.
- `Source/`: 203 Dateien einschließlich C++-Gameplay und Unreal Game-/Editor-/Server-Targets.
- `Content/`: 1.074 Dateien einschließlich 1.037 Unreal-Assets und Maps.
- `Config/`: vier Unreal-Konfigurationsdateien.
- Bestehende Charakter-, Inventar-, Kampf-, Quest-, Welt-, Multiplayer-, Visual- und Animationssysteme.
- `Source/DarkBloodEditor.Target.cs` ist der gültige Editor-Target; ein separater Editor-Modulordner
  ist nicht erforderlich. Kein projektlokaler `Plugins/`-Ordner vorhanden; Engine-Plugins sind
  in der `.uproject` aktiviert. Das ist eine gültige Unreal-Struktur.

## ENTFERNT / WIEDERHERGESTELLT

Nichts entfernt oder zurückgesetzt: Es gibt keine belegten Minecraft-Fehländerungen.
Keine Unreal-C++-Klasse, Map, Blueprint, Asset- oder Konfigurationsdatei musste repariert werden.
Neu angelegt: `docs/PROJECT_IDENTITY.md`, `AGENTS.md` als Einstieg für künftige Agenten und dieser Bericht.
Keine neue Entwicklungsphase und keine Bossmodellierung begonnen.

## BUILD

**Erfolgreich**, Exitcode **0**, UnrealBuildTool meldet `Result: Succeeded`.
Ausgeführt mit den vorhandenen UE-5.8-Buildtools:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' DarkBloodEditor Win64 Development '-Project=C:\Projekte\dark-Blood\DarkBlood.uproject' -WaitMutex -NoHotReloadFromIDE
```

Fünf Buildaktionen, einschließlich Kompilierung und Linken von `UnrealEditor-DarkBlood.dll`.
Buildzeit laut Tool: 52,08 Sekunden. Keine Regeneration der Projektdateien erforderlich.
Beleg: `Saved/ProjectIdentityAudit/DarkBloodEditor-build.log`.

Eine bestehende Warnung C4996 in `Source/DarkBlood/Private/Debug/DBCheatManager.cpp:952`:
`ACharacter::GetMovementBase` ist veraltet. Sie verhindert den Build nicht und ist keine
Minecraft-Fehländerung; keine ungefragte API-Umstellung in dieser Reparatur.

## PROJEKTSTART

**Editorinitialisierung und Laden der Spielwelt erfolgreich.** Der automatisierte Start lief
mit realem D3D12-Renderer auf der NVIDIA GeForce RTX 4070 SUPER. Das Startskript sah zunächst
die temporäre Editorwelt `/Temp/Untitled_1.Untitled_1` und lud anschließend ausdrücklich
`/Game/DarkBlood/Maps/L_Realm.L_Realm`. Ergebnisdatei: `status: PASS`.
Die Unreal-Kartenprüfung meldete **0 Fehler und 0 Warnungen**. Der Editor wurde über
`quit_editor()` regulär beendet; das Log bestätigt `Editor shut down` und `LogExit: Exiting`.
Keine Speicherung der Map angefordert. Dies ist ein automatisierter Editor-/Map-Ladetest,
kein interaktiver Spieltest und keine Bestätigung, dass die konfigurierte Standardkarte
automatisch als `L_Realm` startet.
Belege: `Saved/ProjectIdentityAudit/editor-start.log`, `editor_start_check.py` und
`editor-start-result.json`.

Der Editor fügte trotz `-NoSaveSettings` einen AndroidFileServer-Konfigurationsabschnitt
zu `Config/DefaultEngine.ini` hinzu. Diese ausschließlich vom Test erzeugte Ergänzung
wurde nach Vergleich mit dem sauberen Ausgangsstand zurückgenommen. Die erzeugte Fassung
liegt lokal gesichert unter `Saved/ProjectIdentityAudit/DefaultEngine.editor-generated.backup.ini`.
Abschließend sind `Source/`, `Content/`, `Config/` und die `.uproject` gegenüber HEAD unverändert.

## OFFEN / GRENZEN

- Das Startlog enthält 17 `LogAutomationTest: Error: Condition failed`-Meldungen
  während der Initialisierung, vor Ausführung des Prüfscripts. Der Logabschnitt nennt keinen
  konkreten Test oder Stacktrace. Ursache ungeklärt; kein belegter Minecraft-Bezug. Sie verhinderten
  weder Editorinitialisierung noch Map-Laden oder reguläres Beenden. Daher kein Anspruch auf
  einen vollständig fehlerfreien Startlog und keine bestandene allgemeine Automation-Testsuite.
- `EditorStartupMap` und `GameDefaultMap` zeigen weiterhin auf die Engine-OpenWorld-Templatekarte.
  Die vorhandene `L_Realm` wird deshalb zusätzlich ausdrücklich geladen. Keine Umstellung der
  Startkarten als Teil dieser begrenzten Reparatur.
- Die README enthält noch den historischen Hinweis, UE sei nicht installiert und nie kompiliert
  worden. Für diese lokale Prüfung gelten die oben dokumentierten aktuellen Buildbelege.
- Ein Editor-/Map-Ladetest ist keine vollständige Gameplay-, Multiplayer- oder Grafikabnahme.
- Neue Dokumentationsdateien sind lokale, noch nicht eingecheckte Änderungen. Kein Push durchgeführt.
