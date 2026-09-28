# VISUAL FOUNDATION – Status (Phase 5.5)

Stand 27.09.2026. Umsetzung des Codex-Pakets `DARK_BLOOD_UE58_VISUAL_FOUNDATION_PACK_V2` (Kopie: `docs/VisualPack/`)
nach 18_CLAUDE_VISUAL_CHARACTER_TASK.md und 01_CLAUDE_MASTER_TASK.md. Technik: [VISUAL_FOUNDATION.md](VISUAL_FOUNDATION.md).

**Kurzfassung:** Das komplette Fundament (Daten, Systeme, Materialsystem, modularer Baukasten, Vegetationsregeln,
Licht, Visual Slice) ist gebaut und getestet. **Poly-Haven-CC0-Assets sind integriert** (17 Textursets, 10 Modelle:
Laubbaum, toter Stamm, Büsche, Farne, Moos, Felsen, Findling, Baumstumpf). Die Welt hat damit echte Materialien und
Vegetation; dazu **ambientCG-CC0-Materialien** (Lackholz rot/schwarz, Shoji-/Laternenpapier, Leinen, rote/indigo Stoffe,
Eisen, Bronze, Bambus), **Kirschbäume** (Inselbaum-Geometrie mit umgefärbten Blütenblättern) und **Tannen**. Es fehlen noch
MetaHumans, Animationen und Architektur-Module (Fab/Epic, Konto nötig) – dafür sind die Slots vorbereitet; es muss nichts neu programmiert werden.

## ✅ Integriert

| Bereich | Ergebnis |
|---|---|
| Charakter-Fundament | Profil-Asset (Schema aus dem Paket), Visual-Komponente an allen Figuren, Slots Face/Hair/Beard/Outfit/Armor/Accessory, Qualitätsstufen Player/Hero/ImportantNpc/Crowd, nur Ids repliziert, Greybox-Rückfall |
| Charaktererstellung | Frisur, Haarfarbe, Augenfarbe, Hautton, Narben (gespeichert, im Koop übertragen) |
| Figuren im Spiel | animierte Körper (Engine-Mannequin als DEV-Stand) für Spieler (Klassenfarbe), König, Hauptmann, NPCs, Dämonen (Dark-Blood-Adern-Material) |
| Animations-Fundament | Animations-Sets nach `Anim.*`-Tags, Fähigkeiten auf Schlüssel umgestellt, Abspielrate an Gameplay-Timing, Tod-Montage, AnimInstance-Basis (Locomotion, Kampfzustände, Lock-on, Look-at, Emotion, Blinzeln), Root Motion aus |
| Gesichts-Fundament | Look-at (NPCs schauen nahe Spieler an), Emotion, prozedurales Blinzeln als AnimBP-Variablen |
| Materialsystem | 12 Master (Holz, Stein, Putz, Dach, Metall, Stoff, Vegetation, Nässe, Haut, Dark Blood, Decal, Landschaft mit 8 Layern) und 40 Instanzen; Makro-/Detail-Variation, Moos, Schmutz, Nässe, Verderbnis, Texturweg (UV/world-aligned) |
| Architektur | modularer Baukasten mit 10 Gebäudetypen, 3 Dachfamilien, Kit-Schnittstelle nach Schema; Tore, Laternen, Wege, Zäune, Mauern, Bach, Brücke, Dungeon-Eingang |
| Vegetation | regelbasierte Scatter-Volumen (Hang, Straßen, Siedlung, Landmarken, Randausdünnung, 8 Biome), deterministisch im Koop |
| Licht | Tag, Dämmerung, Nacht, Dämonennacht: Sonne/Mond, Skylight, volumetrischer Nebel, Grading, Laternenlicht |
| Visual Slice | Hauptstadt-Hof, Straße, Dorf mit Taverne und Innenraum, Wald, Bach mit Brücke, Dungeon-Eingang, Schrein |
| Poly Haven (CC0) | Texturen: weathered_planks, hinoki_planks, old_planks_02, plastered_wall_02, clay_plaster, grey_roof_01, reed_roof_04, japanese_stone_wall, mossy_rock, lichen_rock, rock_pitted_mossy, grey_stone_path, forest_leaves_04, rocky_trail_02, japanese_cedar_bark, sakura_bark, burned_ground_01 (2K) → 22 Material-Instanzen; Modelle (1K, Nanite): tree_small_02, dead_tree_trunk_02, shrub_02/04, fern_02, moss_01, rock_moss_set_01/02, boulder_01, tree_stump_01 → Wald, Wegränder, Bachufer, Dungeon-Hügel |
| ambientCG (CC0) | PaintedWood003/005, Paper001/004, Fabric036/026/023, Metal009, Metal035, Bamboo002A (2K) → 12 Instanzen; Poly Haven zusätzlich island_tree_02 (Wald + Kirschbaum mit `MI_DB_Foliage_Sakura_Leaves`), fir_sapling_medium (Bergwald) |
| Waffen | Katana in der rechten Hand (Fab „Corrupted Dark Katana“, lokal), zehn Klingen-Oberflächen (`M_DB_Katana_Master`: Verderbnis-Flecken umgefärbt + pulsierendes Glühen je Element), Rückfall Stahlklinge |
| Grafik / 4K | Menü `[F10]`: Qualität Niedrig–Kino, Auflösung bis 3840×2160, Anzeigemodus, TSR-Hochskalierung (100/67/58/50/33 %), Hardware-Raytracing (Lumen, SM6), FPS-Limit, VSync; empfohlene Werte beim ersten Start (`UDBGameUserSettings`) |
| Horizont | selbst erzeugte Bergketten (3,5–14 km, bis ~3 km hoch, Schnee über der Schneegrenze) und Hügel (0,55–4 km), Nanite, Luftperspektive je Tageszeit, Lücke zur Küste im Süden |
| Wiesen / Gras | Wiesenboden bis zu den Hügeln, Waldboden unter den Wäldern, Poly-Haven-Grasbüschel (Nanite, Foliage-Material mit Alpha) per Scatter-Volumen (`GrassDensity`, Biom Meadow) |
| Atmosphäre | Lichtstrahlen (volumetrische Streuung + Light-Shaft-Bloom), Wolken/Himmel (ergänzt, falls der Level keine hat), Mond nachts, Blutmond in der Dämonennacht |
| Bewegte Luft | `ADBAmbientFx`: Kirschblüten, Glut (Schmiede, Dämonentor, Dämonenbaum), Glühwürmchen (Dämmerung/Nacht), rote Asche |
| Dämonengebiet | nördlich des Dungeons: Dämonenbäume (Adern-Rinde, glühende Blätter), zwei verdorbene Torii, Blutfluss (`MI_DB_Blood_River`), Ruinen, roter lokaler Nebel, Lichter brennen auch am Tag |
| Werkzeuge | Setup-Skripte, Material-Generator, Poly-Haven- und ambientCG-Download + Import, Ordner-Setup, Audit (Paket + `DBVisualAudit`), `DBPerfSnapshot`, `DBView` |

## 🟡 Vorbereitet (Slot da, Inhalt fehlt)

- Kit-Module (`DBBuildingKit`) – wartet auf echte Gebäudemodule (Fab-Torii/Laterne, eigene Blender-Module).
- Bambus: Halme aus Primitiven mit ambientCG-Bambustextur (kein CC0-Bambusmodell ohne Konto); Glyzinie fehlt.
- MetaHuman-Profile (Hero/NPC/Crowd), Haar-/Bart-Teile, Haut-/Haar-/Augen-Parameter.
- Eigene AnimBPs auf `UDBAnimInstance` (Motion Matching, Kampf-Locomotion, Fuß-IK, Motion Warping auf Ziele).
- Landschaft (`M_DB_Landscape_Master`) – es gibt noch kein Landscape; die Hauptwelt kommt in Phase 6.
- PCG-Graphen: Regeln sind im Scatter-Volumen umgesetzt; echte PCG-Graphen lohnen erst mit Vegetations-Meshes.

## ⬜ Fehlt wegen Asset

Bambus-/Glyzinien-Modelle, Torii-/Laternen-/Statuen-Modelle, Dachziegel-Module, MetaHumans, Haare,
Kleidung, Rüstungen, Waffen-Meshes, Katana-Animationen, Gesichtsanimation, VFX (Niagara), Wasser-Shader, HDRIs.
Bis dahin: Baukasten-Bauteile mit echten Materialien + Engine-Mannequin (klar als DEV markiert).

## Zielbilder

`docs/VisualPack/Reference/`: DarkBlood_Zielbild.png (8 Bereiche), DarkBlood_Regionen.png (16 Regionen),
DarkBlood_Siedlungen.png (16 Siedlungstypen), DarkBlood_Weltkarte.png (Karte). Weltgröße festgelegt: 16 × 16 km
mit allen 16 Regionen in der Anordnung der Karte.

## ✋ Manuell nötig (nur du kannst das)

1. ~~Poly Haven~~ – erledigt (siehe oben). Weitere Rechner: `python Tools/UE58/polyhaven_fetch.py`, dann
   `db_create_material_foundation.py` und `db_import_polyhaven.py` (die importierten Assets liegen bereits im Repo).
2. **Fab** (Konto nötig): Free-Assets aus dem Manifest „Zu Projekt hinzufügen" (Torii, Wandlaterne, Statuen,
   European Forest, Pine Forest Road). Vorher Lizenz und „Allows usage with AI" auf der Seite prüfen.
3. **Game Animation Sample 5.8** über Fab/Epic laden, in dieses Projekt migrieren.
4. **MetaHuman Creator** in UE 5.8: Spielerbasis A/B, König, Hauptmann, 2–3 Crowd-Varianten erstellen.
5. Einmal `Tools\UE58\Setup-DevMannequin.ps1` auf jedem Rechner (bereits auf diesem erledigt).
6. `git push` (die Commits liegen lokal).

## ⛔ Lizenz-/AI-Blocker

- **NO-AI-Assets** (03_MANUAL_ONLY_HIGH_END.md): Quixel Japanese Shrine Stone Floor, Japanese Stone Lantern, Giant
  Bamboo, Digikore Japanese Temple / Samurai Town. Nicht an Claude/Codex geben. Du fügst sie im Editor ein und trägst
  sie in die Slots ein: Stein-Tōrō → `ADBLantern`/Kit-Modul, Bambus → `ADBScatterVolume` (Biome Bamboo, `Plants`),
  Schreinboden → Material-Instanz `MI_DB_Stone_Temple` bzw. Kit.
- **Engine-Mannequin**: stammt aus deiner UE-Installation und wird nicht ins öffentliche Repo kopiert
  (`.gitignore`), sondern per Skript lokal erzeugt.
- **Korean-Heritage-Assets** (KHS) nur als Nebenbezug, nicht als historisches Japan ausgeben.
- **Fab-Assets** dürfen nicht roh ins öffentliche Repo, wenn die Lizenz Weitergabe verbietet → im Zweifel wie das
  Mannequin per `.gitignore` lokal halten.

## Performance (gerenderter Test, 1600×900, Editor-Build, Dämmerung/Tag)

| Messung | Wert |
|---|---|
| Frame / GPU (mit Poly-Haven-Texturen und Nanite-Vegetation) | 7,2–9,5 ms / 5,6–6,7 ms (≈ 105–138 FPS) |
| Draw Calls | 203–289 |
| Slice | 50 Akteure, 361 Instanz-Komponenten, ≈ 3 700–4 400 Instanzen, 25 Punktlichter (ohne Schatten) |
| Texturen | 17 × 3 Karten 2K (Streaming), Laub 1K |
| Aufbau | 120–240 ms einmalig pro Rechner |

Noch nicht gemessen: Texture-Pool/VRAM (keine Texturen), HLOD/World-Partition-Streaming (keine Hauptwelt), Nanite
(Primitive sind nicht Nanite; echte Meshes bekommen Nanite beim Import).

## Tests

- Build Editor Win64 Development (Unity und Non-Unity) ohne Fehler/Warnungen; Regelkern 44/44.
- Headless-Regression: Kombos, schwerer/aufgeladener Angriff, Block/Parry/Dodge, Klassen-Kit (Krieger), Dämonenkampf,
  Truhe/Schmieden/Reparatur – Ergebnisse wie vor der Änderung.
- Koop (Listen-Server + Client): identischer Slice auf beiden (50 Akteure / 4 522 Instanzen), `DBVisualSlice` und
  `DBTimeOfDay` repliziert, Kampf auf dem Client normal, Audit 0 fehlende Referenzen.
- Gerendert: Hof, Dorf, Wald, Brücke, Schrein, Dungeon; Tag/Dämmerung/Nacht/Dämonennacht; Charaktererstellung;
  erneut mit Poly-Haven-Texturen/-Modellen (Regression, Koop und Audit danach wiederholt: unverändert, 0 fehlende Referenzen).
- Poly-Haven-Import: Laub/Moos waren im glTF als transparent markiert (Nanite rendert das nicht) und die Alpha-Maske
  lag separat → eigene Foliage-Materialien mit Alpha-Karte, Nanite „Preserve Area" für Blätter.
- Gefunden und behoben: Boden-Sonde traf die Spielerkapsel bzw. übersah den (WorldDynamic-)Boden; Palast stand im
  Spawn-Bereich der Test-Dämonen → Hof neu angeordnet; Nacht zu hell, Verderbnis zu flächig → Belichtung/Adern
  angepasst; Foliage-Material kompilierte nicht (Wind-Eingang).
- Bekannt (vorher schon so): Mit `-DBDevSlice` bleibt ein 8 m vor dem Start gespawnter Dämon am König hängen
  (direkte KI-Steuerung ohne NavMesh, Phase 6).

## Nächste Schritte

1. Fab-Free-Assets (Torii, Wandlaterne, Statuen, European Forest) → Kit-Module und Scatter-Einträge.
2. Bambus-Modell (Megascans Giant Bamboo, NO-AI → manuell) → `Plants` des Bioms Bamboo.
3. Game Animation Sample → AnimBP auf `UDBAnimInstance`, Set `AS_Player` statt Mannequin-DEV-Set.
4. MetaHumans → Profile `CV_Player_TypeA/B`, `CV_NPC_King`, Crowd.
5. Danach Abnahme nach 19_VISUAL_ACCEPTANCE_CHECKLIST.md und erst dann Phase 6 (Open World, NavMesh, Landscape).
