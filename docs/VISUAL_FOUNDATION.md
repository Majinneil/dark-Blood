# Visual Foundation (Phase 5.5)

Technische Grundlage für den Grafik-Pass nach dem Codex-Paket `docs/VisualPack/` (00–20). Stand und offene Punkte:
[VISUAL_FOUNDATION_STATUS.md](VISUAL_FOUNDATION_STATUS.md).

**Grundsatz:** Alles Sichtbare hängt an austauschbaren Daten (Profile, Material-Instanzen, Kits, Scatter-Einträge).
Gameplay liest nichts davon. Fehlt ein Asset, bleibt der Greybox-Ersatz stehen. Jede Ersetzung ist umkehrbar.

## Einrichtung (einmal pro Rechner)

| Schritt | Befehl | Ergebnis |
|---|---|---|
| Entwicklungs-Figuren | `powershell -ExecutionPolicy Bypass -File Tools\UE58\Setup-DevMannequin.ps1` | kopiert das Mannequin der eigenen UE-5.8-Installation nach `Content/Characters/Mannequins` (nicht im Git) und baut die DEV-Montages `/Game/DarkBlood/Dev/Mannequin` |
| Materialsystem neu erzeugen | `UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_create_material_foundation.py` | Master-Materialien + Instanzen (liegen bereits im Repo) |
| Ordnerstruktur | `Tools/UE58/setup_darkblood_visual_folders.py` | `/Game/DarkBlood/...` laut Paket 06 |
| Poly-Haven-Assets laden | `python Tools/UE58/polyhaven_fetch.py` | CC0-Texturen/-Modelle nach `SourceArt/PolyHaven` (nicht im Git) |
| Poly-Haven importieren | `UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_import_polyhaven.py` | Texturen, Modelle (Nanite), Foliage-Materialien, Instanzen auf Texturweg – **nach** dem Material-Generator ausführen |
| Asset-Audit (Editor) | `Tools/UE58/audit_visual_assets.py` | listet DEV/Platzhalter-Assets |

Ohne Schritt 1 läuft das Spiel unverändert mit Zylinder-Körpern.

## Charaktere

- **`UDBCharacterVisualDefinition`** (Primary Asset `DBCharacterVisual`, `/Game/DarkBlood/Characters/Profiles`) –
  entspricht `character_visual_profile.schema.json`: Qualitätsstufe (Player/Hero/ImportantNpc/Crowd), Body/Face/Skin-
  Preset-Ids, Body-Mesh, Material-Overrides, modulare Teile (Face/Hair/Beard/Outfit/Armor/Accessory, Leader Pose),
  Frisur-/Gesichtsoptionen, Hautton-Palette, Parameternamen für Haut/Haar/Augen/Outfit-Tönung, Klassenfarben,
  Blinzeln/Look-at/Grundemotion, Animations-Set.
- **`UDBCharacterVisualComponent`** an jedem `ADBCharacterBase`: wendet das Profil lokal an (Server und Clients aus
  denselben replizierten Ids), Qualitätsstufe → Anim-Tick/URO, Mesh ohne Kollision, Root Motion ignoriert,
  Tod-/Wiederbelebungs-Präsentation, Look-at-Ziel und Emotion. Dedicated Server lädt keine Meshes.
- **Aussehen** bleibt `FDBAppearance` (gespeichert, repliziert): Charaktererstellung hat jetzt Frisur, Haarfarbe,
  Augenfarbe, Hautton, Narben. Spieler nutzen `CV_Player_TypeA/B`, NPCs `CV_<NpcId>` (sonst `CV_NPC_Default`),
  Dämonen `CV_Enemy_LesserDemon`.
- **MetaHuman-Weg:** MetaHuman im eigenen Projekt erstellen → Profil-Asset mit Body-Mesh (Crowd: Joints-Only-Rig),
  Haar-/Bart-Grooms als Teile, Material-Parameternamen eintragen. Keine Laufzeitabhängigkeit von MetaHuman-Editor-Tools.
- Umschalten: `-DBGreybox` oder `DBVisuals 0|1` (lokal).

## Animation

- **`UDBAnimationSetDefinition`** (`DBAnimationSet`, `/Game/DarkBlood/Animation/Sets`): Montages nach `Anim.*`-Tags,
  Varianten pro Combo-Schritt, Fallback auf den Eltern-Tag (`Anim.Attack.Sprint` → `Anim.Attack`), Anpassung der
  Abspielrate an die Gameplay-Dauer, Locomotion-AnimBP.
- Fähigkeiten spielen nur noch Schlüssel ab (`PlayPresentationMontage`): Light/Heavy/Charged/Air/Sprint/Dash, Block,
  Dodge, HitReact, Parried, Knockdown; Tod über das Profil. **Treffer, i-Frames und Bewegung bleiben serverseitig in
  den Fähigkeiten** – Animation entscheidet nie über Schaden.
- **`UDBAnimInstance`**: Basisklasse für eigene AnimBPs (Game Animation Sample / Motion Matching): Geschwindigkeit,
  Richtung, Luft/Sprint/Flug, Kampfzustände aus GAS-Tags, Kampfhaltung, Lock-on, Look-at (Dialogpartner/Ziel),
  Emotion, prozedurales Blinzeln. Motion Warping ist als Plugin aktiv.
- DEV-Set `AS_Dev_Mannequin`: Template-Angriffe, Dash, Treffer, Tod (in-place, root-locked).

## Materialien

Ein gemeinsamer Oberflächen-Graph für alle Master (`/Game/DarkBlood/Art/Materials/Master`): `M_DB_Wood/Stone/Plaster/
Roof/Metal/Fabric/Foliage/Wetness/Skin_Support/DarkBlood_Master`, dazu `M_DB_Decal_Master` und
`M_DB_Landscape_Master` (8 Layer: ForestFloor, Earth, StonePath, Rock, Mud, Moss, Ash, Snow).

Parameter: BaseTint/TintVariation, Makro- und Detail-Variation (auch ohne Texturen), Roughness-Spanne, Metallic,
Specular, **UseTextures** (BaseColor/Normal/ORM, UV0 oder **WorldAligned**), DirtAmount, **Moos auf Oberseiten**
(world-aligned), **Wetness**, **Dark-Blood-Verderbnis** (pulsierende Adern, Emissive, gezielt statt Rotfilter),
Emissive, Wind (Foliage). 40 Instanzen, z. B. `MI_DB_Wood_Weathered_Dark`, `MI_DB_Stone_Mossy`,
`MI_DB_Roof_Tile_Dark`, `MI_DB_Paper_Shoji_Lit`, `MI_DB_DarkBlood_Veins`.

Poly-Haven-Texturen sind eingebunden (Tabelle in VISUAL_FOUNDATION_STATUS.md). Neue Texturen: in der Instanz
`UseTextures` an, `T_BaseColor/T_Normal/T_ORM` setzen, fertig – das ganze Viertel wechselt mit. Foliage-Master hat
zusätzlich `T_Opacity`. Code greift nur über `EDBArtMaterial`-Slots zu (`UDBArtMaterialSubsystem`).

## Architektur

- **`ADBModularBuilding`**: Sockel, Pfosten-Riegel-Rahmen, Wandfelder (Putz, Shoji, Bretter, Fenster mit Gitter,
  Schiebetür halb offen ≥ 1 m, Ladenfront mit Noren), Satteldach/Walmdach/gestuftes Tempeldach mit Ziegelrippen,
  First, Traufbrettern, Zwischendächer bei Obergeschossen, Veranda, Treppen, Laternen mit Licht, Typ-Ausstattung
  (Tavernen-Innenraum, Esse + Schornstein, Waren, Schrein-Seil + Opferkasten, Feuerholz).
  Parameter: Typ (Wohnhaus klein/groß, Händler, Taverne, Schmiede, Lager, Wachhaus, Tempelhalle, Schrein,
  Dorfhalle), Seed, Region (Capital/Village/Temple/DarkBlood), Wohlstand, Schaden, Raster, Stockwerke, Dachfamilie,
  Veranda, Laternendichte.
- **`UDBBuildingKitDefinition`** (`DBBuildingKit`, Schema `building_module.schema.json`): echte Module pro Kategorie
  ersetzen die Primitive an denselben Rasterpunkten (Pfosten, Wand, Tür, Fenster; Pivot unten mittig, +X außen).
- Weitere Bauteile: `ADBGate` (Torii nur an Schreinen, Dachtor), `ADBLantern` (Stein-Tōrō, Pfostenlaterne),
  `ADBSplineDressing` (Trittsteinweg, Straße, Holz-/Bambuszaun, Burgmauer mit Ziegelkappe, Bach), `ADBBridge`,
  `ADBDungeonEntrance`, `ADBGroundPatch`.
- Alles als Instanced Static Meshes (ein Draw-Call-Bündel pro Mesh/Material), Kollision nur für Tragendes.

## Vegetation (PCG-Regeln)

Mesh-Sets (`EDBArtMeshSet`: Tree, DeadTree, Shrub, Fern, Moss, Rock, Boulder, Stump) liefern die importierten
Poly-Haven-Modelle für Scatter, Weg-/Bachränder und den Dungeon-Hügel; fehlen sie, greifen Primitive.

`ADBScatterVolume`: Biome TemperateForest, CherryGrove, MountainForest, WetForest, Bamboo, Roadside, ShrineGarden,
Corrupted. Regeln: Hangneigung, Straßen-/Wegabstand, Gebäudeabstand, Landmarken-Abstand, Randausdünnung,
deterministischer Seed (Server und Clients platzieren identisch). `Plants`/`Undergrowth` nehmen echte Meshes
(CC0-Bäume, Fab); leer = DEV-Stellvertreter je Biom. Die Regeln sind so gebaut, dass sie 1:1 in PCG-Graphen
übertragen werden können, sobald Vegetation-Assets da sind.

## Licht

`ADBVisualSliceDirector` setzt Tag / Dämmerung / Nacht / Dämonennacht: Sonne/Mond (Richtung, Farbe, Stärke),
Skylight, Höhennebel + volumetrischer Nebel, Post-Process (Belichtungsgrenzen pro Stimmung, zurückhaltender Bloom,
Vignette, Grading). Laternen- und Innenlichter nur in Dämmerung/Nacht, ohne Schatten (Kosten). Lumen/VSM bleiben
rein visuell.

## Visual Slice

`-DBDevSlice` baut um den Story-Slice: Burghof der Hauptstadt (Palasthalle, Schmiede, Wachhaus, Mauern, Tor,
Laternen, Kirschbaum), Straße, Dorf (Taverne mit Innenraum, Händler, Häuser, Lager, Zaun), Wald, Bach mit
Lackbrücke, Dungeon-Eingang mit Dark-Blood-Verderbnis, Schrein mit Torii, Tōrō, Kirschen, Bambus, Bergwald.
Gameplay-Akteure werden nicht bewegt; der Hof vor dem Start bleibt frei (Kampfraum).

Umschalten: `-DBNoVisualSlice`, `DBVisualSlice 0|1`, Stimmung: `-DBTimeOfDay=Night`, `DBTimeOfDay Day|Dusk|Night|DemonNight`.

## Kommandos

| Kommando | Wirkung |
|---|---|
| `DBVisualSlice 0\|1` | Slice auf allen Rechnern entfernen/aufbauen (Server) |
| `DBTimeOfDay <Preset>` | Day, Dusk, Night, DemonNight (Server, repliziert) |
| `DBVisuals 0\|1` | Charakter-Profile an / Greybox-Körper (lokal) |
| `DBView X Y Yaw [Pitch]` | Spieler relativ zum Slice-Ursprung platzieren (Screenshots) |
| `DBPerfSnapshot` | FPS, Frame-/GPU-Zeit, Draw Calls, Primitive, Slice-Statistik |
| `DBVisualAudit` | fehlende Material-Slots, Profile, Montages |
