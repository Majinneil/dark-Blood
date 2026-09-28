# Weltdesign

## Grundsatz

Eine einzige, nahtlose Hauptwelt (`/Game/DarkBlood/Maps/L_Realm`, World Partition). Keine Portale zwischen
Regionen, keine Levelauswahl. Dungeons sind Teil der Welt; nur bei technischem Bedarf werden einzelne
Innenräume über Level Instances / Data Layers gestreamt.

**Status:** Die Hauptwelt `/Game/DarkBlood/Maps/L_Realm` (16 × 16 km) wird vom Commandlet `DBBuildRealm` aus dem
Weltplan erzeugt (`UnrealEditor-Cmd DarkBlood.uproject -run=DBBuildRealm`, Vorschau mit `-Preview` nach
`Saved/Realm/RealmPreview.bmp`). Vorlage: `docs/VisualPack/Reference/DarkBlood_Weltkarte.png`.

## Die 16 Gebiete der Weltkarte

Norden oben, Osten rechts. Lage, Gelände, Bodenschichten und Flüsse stehen in `Source/DarkBlood/.../World/DBRealmLayout.cpp`.

| ID | Gebiet (Karte) | Vasallen-Thema | Gelände |
|---|---|---|---|
| `Capital` | Hauptstadt | Schutzzauber | Plateau (70 m) im Norden, Hafen an der Nordküste |
| `Region01` | Kirschblütental | Blut | Täler, Kirschhaine |
| `Region02` | Eisöde | Frost | Hochplateau, Schnee, Tannen |
| `Region03` | Bambuswälder | Schatten | dichter Wald |
| `Region04` | Nebelberge | Donner | Gebirge bis ~1 000 m, Schnee |
| `Region05` | Feuergebirge | Feuer / Asche | vulkanischer Fels, Lavaströme |
| `Region06` | Wüstenlande | Knochen / Tod | Dünen, Oase |
| `Region07` | Flusslande | Gift / Seuche | Flussniederung, Fischerdorf |
| `Region08` | Wald der Geister | Wahnsinn / Illusion | Sumpfwald, Tümpel |
| `Region09` | Reisfelder | Bestien | Terrassen |
| `Region10` | Großstadt | Eisen | Ebene |
| `Region11` | Küstenland | Meer | Küste, Buchten, Hafenstadt |
| `Region12` | Himmelstempel | Wind / Himmel | Seen, Tempelsiedlung |
| `Region13` | Dämonenöde | Finsternis | zerklüftet, verdorben |
| `Region14` | Vasallenfestung | Schwarze Festung | Anhöhe, Grenzposten |
| `TheEnd` | Das Ende | Dunkles Blut | Spitzen, verdorben, Lava |

Siedlungen (je Typ eine, `DBRealm::GetSettlements`): Hauptstadt(bezirke), Hauptstadthafen, Großstadt, Dorf, Bergdorf,
Hafenstadt, Reisdorf, Waldsiedlung, Bergwerksstadt, Tempelsiedlung, Grenzposten, Karawanenstadt, Tavernenstadt,
Fischerdorf, Schneesiedlung, Flusssiedlung, Oasenstadt. Der Boden ist dort eingeebnet (Häfen behalten ihr Becken);
`ADBRealmDirector` baut sie auf jedem Rechner deterministisch, sobald die Kamera näher als 2,8 km ist, und entfernt
sie ab 3,5 km. Straßen werden zuerst geplant, dann stehen Häuser beidseitig, und eine Blockfüllung setzt Hinterhäuser
zwischen die Straßen (dicht in Städten, locker in Dörfern), ohne Kreuzungen zu verbauen. Die Häuser entstehen danach
über mehrere Frames (3 ms Budget) in gemeinsame Instanz-Blöcke zu je 40 Häusern: Die Großstadt (≈1100 Häuser,
220 000 Teile) ist so ~480 Primitive statt 13 600 und erscheint ohne Ruckler.

Schiffe: In jedem Hafen liegt eine segelbare Pinasse (`ADBShip`, Steuer mit E, W/S Segel, A/D Ruder, max. 14 m/s). Der
Server bewegt das Schiff, der Steuermann ist angehängt, Mitspieler fahren auf dem Deck mit (repliziert, getestet im
Koop). Bug und Bugflanken prüfen die Wassertiefe, das Schiff läuft nicht auf Land. Testbefehle `DBSpawnShip`,
`DBSail <Ruder> <Segel> <Sekunden>`.

Meer: eine 40-km-Fläche auf Z −0,3 m mit `M_DB_Sea` (dunkel, glänzend, opak; drei gegeneinander driftende
Weltprojektionen der generierten Wellen-Normalmap `Tools/generate_sea_normal.py`). Keine Kollision – Figuren im Meer
laufen derzeit auf dem Meeresgrund (Schwimmen fehlt noch).

Karte: 16 Landscape-Kacheln (4 m Raster, 8 Bodenschichten `M_DB_Realm_Landscape`, Gras über Landscape-Grass-Types,
deren Dichtekarten zur Laufzeit auf der GPU entstehen: `grass.GrassMap.UseRuntimeGeneration=1`),
164 000 Bäume und Felsen in 1-km-Zellen (`ADBRealmVegetation`, Nanite), Meer auf Höhe 0. Nanite-Landscape ist
möglich (`-Nanite`), braucht aber ~590 MB pro Kachel und mehr Speicher als 32 GB beim Bauen – daher aus.
Testbefehl `DBTravel <Region|Siedlung>`.

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
