# Credits

## Poly Haven (CC0)

Texturen und Modelle von [Poly Haven](https://polyhaven.com), Lizenz CC0 (https://polyhaven.com/license), geladen
über `Tools/UE58/polyhaven_fetch.py`:

- Texturen: weathered_planks, hinoki_planks, old_planks_02, plastered_wall_02, clay_plaster, grey_roof_01,
  reed_roof_04, japanese_stone_wall, mossy_rock, lichen_rock, rock_pitted_mossy, grey_stone_path, forest_leaves_04,
  rocky_trail_02, japanese_cedar_bark, sakura_bark, burned_ground_01, aerial_grass_rock, aerial_rocks_02, snow_02,
  cliff_side, leafy_grass, coast_sand_rocks_02
- Modelle: tree_small_02, dead_tree_trunk_02, shrub_02, shrub_04, fern_02, moss_01, rock_moss_set_01,
  rock_moss_set_02, boulder_01, tree_stump_01, island_tree_02, fir_sapling_medium, coastal_cliff_01, coastal_cliff_02,
  rock_face_01, rock_face_02, grass_medium_01, grass_medium_02, dutch_ship_medium, ship_pinnace, modular_wooden_pier,
  wooden_crate_02, wooden_barrels_01, chinese_tea_table, round_wooden_table_01, wooden_stool_02, lantern_chandelier_01
  (Welt-Import: `Tools/UE58/db_import_polyhaven_world.py`)

Eigene Inhalte: die Berge und Hügel am Horizont erzeugt `Tools/UE58/generate_backdrop_terrain.py` (Rauschen, kein
fremdes Material), importiert mit `db_import_backdrop_terrain.py`.

CC0 verlangt keine Nennung; sie steht hier als Dank und zur Herkunftsdokumentation.

## ambientCG (CC0)

Materialien von [ambientCG](https://ambientcg.com), Lizenz CC0 (https://docs.ambientcg.com/license/), geladen über
`Tools/UE58/ambientcg_fetch.py`: PaintedWood003, PaintedWood005, Paper001, Paper004, Fabric036, Fabric026, Fabric023,
Metal009, Metal035, Bamboo002A.

## Fab – CC BY 4.0 (Namensnennung Pflicht, auch im späteren Spiel-Abspann)

Über das Fab-Konto des Projektinhabers geladen, alle mit „Allows usage with AI: Yes“, Lizenz
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Eingebunden unter `/Game/DarkBlood/Art/Fab/`, Materialien an
das DARK-BLOOD-Materialsystem angepasst (Änderung im Sinne von CC BY):

- „Japanese Torii Gate“ – Pikas – https://www.fab.com/listings/805a07ba-a248-4871-bf06-78bea645ba7b
- „Medieval Wall Mounted Lantern“ – Kigha – https://www.fab.com/listings/11f8f6d9-69dd-4119-8490-ebe686ff9626
- „Ibaraki Temple Statue – MTSU Animation“ – Paul Griswold / MTSU Animation – https://www.fab.com/listings/bc48cffd-d102-4049-b200-20ca0982198e
- „Shrine statue in Kusatsu Japan – MTSU“ – Paul Griswold / MTSU Animation – https://www.fab.com/listings/b37529e9-b708-45fa-b399-7adaee4f6e68

## Sketchfab – CC BY 4.0 (Namensnennung Pflicht, auch im späteren Spiel-Abspann)

Kostenlose, herunterladbare Modelle ohne „NoAI“-Markierung, über das Sketchfab-Konto des Projektinhabers geladen
(2026-09-28), Lizenz [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Quellen unter `SourceArt/Sketchfab/`
(nicht im Repository), importiert von `Tools/UE58/db_import_sketchfab.py` nach
`/Game/DarkBlood/Art/Environment/Sketchfab/<Schlüssel>/` (Nanite, Materialien angepasst, skaliert und gedreht –
Änderungen im Sinne von CC BY). Eingesetzt über `DBModelLibrary`.

| Modell | Urheber | Schlüssel | Quelle |
|---|---|---|---|
| „Japanese house "minka"“ | kolya5561 | `minka_houses` | https://sketchfab.com/3d-models/1b757cde000240cb84d571032946bfe6 |
| „Japanese House“ | Daniel Stringer | `japanese_house` | https://sketchfab.com/3d-models/771899f2af8a4b57aa98b1ca1ae83034 |
| „Shirakawago Village Set House 1“ | Lokomotto | `shirakawago_house (importiert, derzeit nicht verwendet)` | https://sketchfab.com/3d-models/f94da738aefc4674ab2062df5c01cd43 |
| „Japanese Temple“ | galaxxxy | `temple_pagoda_lanterns` | https://sketchfab.com/3d-models/5917e267ca3e48ac8a27f9ec5809fc29 |
| „Japanese Temple“ | Jainesh Pathak | `japanese_temple` | https://sketchfab.com/3d-models/a210febbec4f454dbd0df1d142be06bc |
| „Kokura Castle“ | AVATTA | `kokura_castle` | https://sketchfab.com/3d-models/aba23531911c45439067a6e0aaccad07 |
| „Japanese Castle“ | Zorodroger | `japanese_castle` | https://sketchfab.com/3d-models/a37d9de4c8dc4f2da25698bb2028b10a |
| „Chinese Junk Ship“ | pinkycolada | `junk_red_large (Kriegsschiff)` | https://sketchfab.com/3d-models/35b340bce9fb4e0680bc0116cebc35c9 |
| „junk“ | NorbertNagy | `junk_red_small (Kampfschiff)` | https://sketchfab.com/3d-models/7350d87350ab41f2be0d0640d2030518 |
| „Chinese Junk Ship Model“ | BG Builds | `junk_merchant (Handelsschiff)` | https://sketchfab.com/3d-models/dca48504de4e4794b790e249983238cc |
| „Wooden Boat (PBR - Game ready)“ | Javaad | `wooden_boat (Kleines Boot)` | https://sketchfab.com/3d-models/863f20dcd6324b799b69e6a583909a6b |
| „Pagoda“ | Juoda | `pagoda` | https://sketchfab.com/3d-models/b8c35388ecd443178c24a5e5cb4c972c |
| „Japanese Lantern“ | Joseph Casetta | `lantern_hanging` | https://sketchfab.com/3d-models/2ca3f1b33e0d41fca89718ec5932e5e2 |
| „Stone Japanese Lantern“ | magiccc | `lantern_stone` | https://sketchfab.com/3d-models/6b475ce70edc4399b4364cc36afe3c9a |
| „Asian Shrine“ | Pikas | `asian_shrine` | https://sketchfab.com/3d-models/e9c5a53fdb09482ab64ac3024b0e2522 |
| „Traditional Japanese Bridge“ | matthewnixon | `bridge_red` | https://sketchfab.com/3d-models/7a58c496216f42bca9a37d725be2ef0f |
| „Japanese Torii gate Game Asset“ | Bazylonator | `torii_game` | https://sketchfab.com/3d-models/e12d2fa1b2b94928b8b87cb7787e2462 |
| „Torii gate“ | blash3D | `torii_large` | https://sketchfab.com/3d-models/abbd7a053bd84a08a207ca86bdc62783 |
| „bamboo“ | evolveduk | `bamboo_small (importiert, derzeit nicht verwendet)` | https://sketchfab.com/3d-models/a02bf0e3ffe44617ad49daf3cd94fe59 |
| „Bamboo“ | riysstech | `bamboo_stalks (Bambushaine)` | https://sketchfab.com/3d-models/0efc022837db43beb4c757155f004ec6 |
| „Bamboo“ | arthur | `bamboo_clump` | https://sketchfab.com/3d-models/b2e6f889630e4ab593376a151836a3e1 |
| „Japanese Cherry Tree (medium-Poly)“ | Sereib | `cherry_tree` | https://sketchfab.com/3d-models/e0306a4402b44fa08f55aa58518dcb9c |
| „Japanese Black Pine“ | matt z chan | `black_pine (importiert, derzeit nicht verwendet)` | https://sketchfab.com/3d-models/f0cb4705f1c446c7bc393fdbfcdf024a |
| „Cedar tree“ | Georgeous | `cedar_tree` | https://sketchfab.com/3d-models/adf5bdebd05340659dae92219a63f62d |
| „Western Red Cedar *Inspired* Tree - SPRING Ver.“ | Sir Sonat | `red_cedar` | https://sketchfab.com/3d-models/82d8305f9fd74687bedaa8be36e292d6 |
| „Japanese Maple“ | endlessvoidmc | `maple_b` | https://sketchfab.com/3d-models/003c6ab20e644655ba36fe54cb04765b |
| „Jap Maple“ | kelvladmail | `maple_c (importiert, derzeit nicht verwendet)` | https://sketchfab.com/3d-models/5e6b338f674e4a6db88763a715342eef |
| „Japanese Red Maple“ | Tokuwa | `maple_red` | https://sketchfab.com/3d-models/7593d53e3e954240ad69220da9a6e9ff |

## Fab – Standard-Lizenz (nicht im Repository)

Fab verbietet die Weitergabe als eigenständige Dateien; jede Person lädt sie aus ihrer eigenen Fab-Bibliothek nach
`SourceArt/Fab` und importiert sie mit `Tools/UE58/db_import_fab.py` (landen in `/Game/DarkBlood/Dev/FabAnims/`, ignoriert):

- „Fight Mocap Animation Data“ – Mocap.in (Lizenz „Persönlich“: kostenlos bis 100.000 US-$ Jahresumsatz)
- „Dynamic Falling & Rolling Animation Pack“ – DZTFIX KATSU (Lizenz „Persönlich“, wie oben)
- „Corrupted Dark Katana | Dark Fantasy Japanese Sword“ – Deepanshu (Lizenz „Persönlich“, wie oben). Nur als
  `.blend`: erst `Tools/UE58/blender_export_katana.py` (Blender 5.x), dann `Tools/UE58/db_import_fab_weapons.py`
  → `/Game/DarkBlood/Dev/FabWeapons/` (ein Klingenmodell, zehn Oberflächen für die Katana-Sammlung)
- In der Bibliothek, Installation über den Epic Launcher: „Kostenloses Animationspaket“ (Gamma Studio),
  „Mage Collection Samples“ (Rapa Motion), „Europäische Waldumwelt“ (Blackridge, CC BY 4.0)
- **Paragon-Figuren von Epic Games** (kostenlos, Standard-Lizenz): Minions, Countess, Greystone, Grux, Kallari,
  Khaimera, Kwang, Morigesh, Rampage, Sevarog, Wukong; dazu „Game Animation Sample“ (Epic Games). Über den Epic Launcher
  mit „Dem Projekt hinzufügen“ nach `Content/Paragon*` (ignoriert); danach `Tools/UE58/db_setup_paragon.py` ausführen:
  Montagen, Animationssets und Aussehensprofile entstehen lokal (`/Game/DarkBlood/Characters/Paragon`,
  `.../Profiles/Paragon`, `/Game/DarkBlood/Animation/Sets/Paragon`, alle ignoriert). Ohne die Pakete behalten Bosse und
  Dämonen ihren Platzhalterkörper.

## Unreal Engine

Entwicklungs-Mannequin und -Animationen stammen aus der lokalen UE-5.8-Installation (Template-Inhalte) und werden
nicht im Repository weitergegeben (`Tools/UE58/Setup-DevMannequin.ps1`).
