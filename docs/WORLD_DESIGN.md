# Weltdesign

## Grundsatz

Eine einzige, nahtlose Hauptwelt (`/Game/DarkBlood/Maps/L_Realm`, World Partition). Keine Portale zwischen
Regionen, keine Levelauswahl. Dungeons sind Teil der Welt; nur bei technischem Bedarf werden einzelne
Innenräume über Level Instances / Data Layers gestreamt.

**Status:** Die Hauptwelt existiert noch nicht (Assets können in der aktuellen Umgebung nicht erstellt
werden). Bis dahin nutzt die Config die World-Partition-Vorlage der Engine (`/Engine/Maps/Templates/OpenWorld`).

## 16 Hauptgebiete (+ Epilog)

| ID (Entwicklung) | Gebiet | Thema | Empf. Stärke (Platzhalter) |
|---|---|---|---|
| `Capital` | Hauptstadt | Schutzzauber, sicherster Ort | 1–5 |
| `Region01` | Vasallengebiet 1 | Blut | 3–9 |
| `Region02` | Vasallengebiet 2 | Frost | 7–13 |
| `Region03` | Vasallengebiet 3 | Schatten | 11–17 |
| `Region04` | Vasallengebiet 4 | Donner | 15–21 |
| `Region05` | Vasallengebiet 5 | Feuer / Asche | 19–25 |
| `Region06` | Vasallengebiet 6 | Knochen / Tod | 23–29 |
| `Region07` | Vasallengebiet 7 | Gift / Seuche | 27–33 |
| `Region08` | Vasallengebiet 8 | Wahnsinn / Illusion | 31–37 |
| `Region09` | Vasallengebiet 9 | Bestien | 35–41 |
| `Region10` | Vasallengebiet 10 | Eisen | 39–45 |
| `Region11` | Vasallengebiet 11 | Meer | 43–49 |
| `Region12` | Vasallengebiet 12 | Wind / Himmel | 47–53 |
| `Region13` | Vasallengebiet 13 | Finsternis | 51–57 |
| `Region14` | Vasallengebiet 14 | Schwarze Festung (rechte Hand des Dämonenkönigs) | 55–61 |
| `TheEnd` | DAS ENDE | Dunkles Blut, Palast des Dämonenkönigs | 62–70 |
| `Paradise` | DAS PARADIES | Epilog, kein Farmgebiet | – |

Reihenfolge und Werte sind Platzhalter; endgültige Namen und Zahlen kommen als `DBRegion`-Assets.

## Regionen im Code

- `UDBRegionDefinition`: ID, Name, Thema, Art (Hauptstadt/Vasall/Ende/Epilog), empfohlene Stärke, Boss-IDs, Musik.
- `ADBRegionVolume`: markiert die Ausdehnung; **nicht räumlich gestreamt**, damit der Server jede Position
  zuordnen kann. Bei Überlappung gewinnt die höhere `Priority` (Hauptstadt in der Provinz).
- `ADBPlayerState::CurrentRegionId` (repliziert, auch für Gruppenanzeige), `DiscoveredRegions`
  (Fog-of-War-Grundlage, nur Besitzer). Erste Entdeckung gibt XP.
- `GetCurrentDanger()` liefert „GEFAHRENSTUFE: EXTREM · Empfohlene Stärke 50–60 · Deine Stärke 13“.
  Das Betreten wird **nie** verhindert.

## Weltzustand

`FWorldState` (Regelkern) → `UDBWorldStateComponent` (GameState):
- Region: besetzt → umkämpft (Zwischenboss) → befreit (Vasall), Dämoneneinfluss 0..1.
- Nach der Befreiung sinkt der Einfluss exponentiell (25 %/Spieltag bis 5 %). Das Ergebnis ist unabhängig
  von der Schrittweite und damit auch für die Offline-Simulation korrekt.
- DAS ENDE öffnet sich nach 14 besiegten Vasallen (Story-Tor, keine künstliche Regionssperre).
- Story-Flags, besiegte Bosse (inkl. Weltbosse).

## Tag/Nacht

Weltuhr auf dem Server; Clients extrapolieren zwischen seltenen Updates (`ClockReplicationInterval`).
Nacht 19:30–05:30. Phase 6/7 hängen Spawnraten, Geschäftszeiten und Siedlungsangriffe daran.

## Hauptstadt-Barriere (Plan, Phase 3/6)

Barriere als Volume mit Filter nach Gameplay-Tag `Faction.Demon`: Menschen, Tiere, Pferde und Kutschen passieren,
Dämonen werden blockiert (Navigation + Kollision + KI-Regel). Visualisierung über Niagara/Material, getrennt von
der Logik.

## Streaming & Landmarken (Plan, Phase 6)

World Partition mit Runtime-Grid (Zellengröße nach Profiling), HLOD für ferne Landmarken (Hauptstadt, Burgen,
Vulkan, Dämonenfestung, Palast). Wichtige Landmarken bleiben über HLOD-Layer immer sichtbar.
