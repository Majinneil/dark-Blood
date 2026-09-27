# DARK BLOOD

Open-World-Dark-Fantasy-Action-RPG mit Koop-Multiplayer auf Basis der **Unreal Engine 5**.
Ein fiktives Reich, inspiriert vom mittelalterlichen Japan: eine geschützte Hauptstadt, 14 Vasallengebiete,
DAS ENDE, der Dämonenkönig und am Ende: **DIE HELDEN DER ZEIT**.

> **Projektstand:** Phase 0 (Preproduction) abgeschlossen, Phase 1 (technisches Fundament) implementiert.
> Der UE5-C++-Code wurde **noch nicht mit der Unreal Engine kompiliert** (in der Entwicklungsumgebung ist keine
> Engine verfügbar). Der engine-unabhängige Regelkern wird mit CMake gebaut und ist durch 30 Unit-Tests abgedeckt.
> Details: [docs/ROADMAP.md](docs/ROADMAP.md)

## Voraussetzungen

| Werkzeug | Version |
|---|---|
| Unreal Engine | **5.8** (Epic Games Launcher; Code nutzt APIs ab 5.5) |
| Visual Studio 2022 (Windows) / Xcode (macOS) / clang (Linux) | laut UE-5.8-Anforderungen (Workload „Spieleentwicklung mit C++“) |
| Git LFS | für `.uasset`/`.umap` und Quelldateien von Assets |
| CMake ≥ 3.20 | nur für die Regelkern-Tests |

Für den Dedicated-Server-Target (`DarkBloodServer`) wird eine aus dem Quellcode gebaute Engine benötigt.

## Schnellstart

```bash
git lfs install
git clone <repo> && cd dark-Blood

# Regelkern bauen und testen (ohne Unreal)
cmake -S Tests/RulesTests -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Unreal: `DarkBlood.uproject` per Rechtsklick → *Generate Visual Studio project files*, dann `DarkBloodEditor`
(Development Editor) bauen und das Projekt öffnen. Ohne eigene Assets startet das Spiel mit klar
markierten **Entwicklungs-Platzhaltern**: Standardkarte „Open World“-Template der Engine, per Code erzeugte
Steuerung, Platzhalter-Klassen/Items/Regionen, Zylinder als Charakterkörper und ein Debug-Overlay.

Koop lokal testen: im Editor *Play → Number of Players = 2, Net Mode = Play As Listen Server*.

### Entwicklerkommandos (Konsole `^` / `~`)

`DBGiveXp 5000`, `DBSetLevel 20`, `DBGiveItem Bag_Adventurer 1`, `DBEquipBag 0 3`, `DBStartQuest MQ01_KingsSummons`,
`DBQuestEvent Talk NPC_King 1`, `DBDefeatBoss Vassal_Region01 Vassal Region01`, `DBSetTime 22`, `DBDamageSelf 9999`,
`DBSaveAll`, `DBDumpCharacter`, `DBDumpWorld`, `DBToggleDebugHUD`. Vollständige Liste:
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#entwicklerkommandos).

## Struktur

```
DarkBlood.uproject
Config/                     Engine-, Spiel-, Input- und Skalierungseinstellungen
Source/
  DarkBloodRules/           engine-unabhängiger Regelkern (C++20, auch per CMake gebaut)
  DarkBlood/                UE5-Gameplay-Modul (GAS, Replikation, Persistenz, Welt)
  DarkBlood*.Target.cs      Game / Editor / Server
Tests/RulesTests/           Unit-Tests für den Regelkern
docs/                       Architektur, Design und Pipelines
```

## Dokumentation

[Architektur](docs/ARCHITECTURE.md) · [Roadmap & Status](docs/ROADMAP.md) · [Game Design](docs/GAME_DESIGN.md) ·
[Weltdesign](docs/WORLD_DESIGN.md) · [Kampfsystem](docs/COMBAT_SYSTEM.md) · [Multiplayer](docs/MULTIPLAYER.md) ·
[Speichersystem](docs/SAVE_SYSTEM.md) · [Quests](docs/QUEST_SYSTEM.md) · [Klassen](docs/CLASS_SYSTEM.md) ·
[Fähigkeiten](docs/ABILITY_SYSTEM.md) · [Siedlungssimulation](docs/SETTLEMENT_SIMULATION.md) ·
[Boss-Framework](docs/BOSS_FRAMEWORK.md) · [Content-Pipeline](docs/CONTENT_PIPELINE.md) ·
[Visual-Pipeline](docs/VISUAL_PIPELINE.md) · [Performance](docs/PERFORMANCE.md)
