# Credits

## Poly Haven (CC0)

Texturen und Modelle von [Poly Haven](https://polyhaven.com), Lizenz CC0 (https://polyhaven.com/license), geladen
über `Tools/UE58/polyhaven_fetch.py`:

- Texturen: weathered_planks, hinoki_planks, old_planks_02, plastered_wall_02, clay_plaster, grey_roof_01,
  reed_roof_04, japanese_stone_wall, mossy_rock, lichen_rock, rock_pitted_mossy, grey_stone_path, forest_leaves_04,
  rocky_trail_02, japanese_cedar_bark, sakura_bark, burned_ground_01
- Modelle: tree_small_02, dead_tree_trunk_02, shrub_02, shrub_04, fern_02, moss_01, rock_moss_set_01,
  rock_moss_set_02, boulder_01, tree_stump_01, island_tree_02, fir_sapling_medium

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

## Unreal Engine

Entwicklungs-Mannequin und -Animationen stammen aus der lokalen UE-5.8-Installation (Template-Inhalte) und werden
nicht im Repository weitergegeben (`Tools/UE58/Setup-DevMannequin.ps1`).
