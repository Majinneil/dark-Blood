# VISUAL FOUNDATION – Status (Phase 5.5)

Stand 27.09.2026. Umsetzung des Codex-Pakets `DARK_BLOOD_UE58_VISUAL_FOUNDATION_PACK_V2` (Kopie: `docs/VisualPack/`)
nach 18_CLAUDE_VISUAL_CHARACTER_TASK.md und 01_CLAUDE_MASTER_TASK.md. Technik: [VISUAL_FOUNDATION.md](VISUAL_FOUNDATION.md).

**Kurzfassung:** Das komplette Fundament (Daten, Systeme, Materialsystem, modularer Baukasten, Vegetationsregeln,
Licht, Visual Slice) ist gebaut und getestet. Die Welt sieht jetzt nach feudaljapanischer Dark Fantasy aus statt nach
Greybox, aber noch **nicht wie das finale Spiel**. Dafür fehlen die eigentlichen Assets (Texturen, Bäume, MetaHumans,
Animationen), die du selbst über dein Epic-/Fab-Konto bzw. Poly Haven holen musst. Sobald sie da sind, werden sie in
Slots eingetragen; es muss nichts neu programmiert werden.

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
| Werkzeuge | Setup-Skripte, Material-Generator, Ordner-Setup, Audit (Paket + `DBVisualAudit`), `DBPerfSnapshot`, `DBView` |

## 🟡 Vorbereitet (Slot da, Inhalt fehlt)

- Texturweg aller Master (`UseTextures`, `WorldAligned`) – wartet auf Poly-Haven-Texturen.
- Kit-Module (`DBBuildingKit`) – wartet auf echte Gebäudemodule (Fab-Torii/Laterne, eigene Blender-Module).
- Scatter-Einträge `Plants`/`Undergrowth` – wartet auf CC0-Bäume (Sakura, Glyzinie, Aprikose …), European Forest.
- MetaHuman-Profile (Hero/NPC/Crowd), Haar-/Bart-Teile, Haut-/Haar-/Augen-Parameter.
- Eigene AnimBPs auf `UDBAnimInstance` (Motion Matching, Kampf-Locomotion, Fuß-IK, Motion Warping auf Ziele).
- Landschaft (`M_DB_Landscape_Master`) – es gibt noch kein Landscape; die Hauptwelt kommt in Phase 6.
- PCG-Graphen: Regeln sind im Scatter-Volumen umgesetzt; echte PCG-Graphen lohnen erst mit Vegetations-Meshes.

## ⬜ Fehlt wegen Asset

Texturen, Bäume/Büsche/Bambus, Felsen, Torii-/Laternen-/Statuen-Modelle, Dachziegel-Module, MetaHumans, Haare,
Kleidung, Rüstungen, Waffen-Meshes, Katana-Animationen, Gesichtsanimation, VFX (Niagara), Wasser-Shader, HDRIs.
Bis dahin: Primitive aus dem Baukasten + Engine-Mannequin (klar als DEV markiert).

## ✋ Manuell nötig (nur du kannst das)

1. **Poly Haven** (CC0, AI-SAFE): die 12 Texturen aus `02_ASSET_MANIFEST_AI_SAFE.csv` laden (2K reicht zum Start).
   Ich kann das auch machen, wenn du es mir erlaubst.
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
| Frame / GPU | 11,7–14,0 ms / 5,0–5,7 ms (≈ 71–86 FPS) |
| Draw Calls | 136–215 |
| Slice | 50 Akteure, 271 Instanz-Komponenten, ≈ 4 500 Instanzen, 25 Punktlichter (ohne Schatten) |
| Aufbau | 120–240 ms einmalig pro Rechner |

Noch nicht gemessen: Texture-Pool/VRAM (keine Texturen), HLOD/World-Partition-Streaming (keine Hauptwelt), Nanite
(Primitive sind nicht Nanite; echte Meshes bekommen Nanite beim Import).

## Tests

- Build Editor Win64 Development (Unity und Non-Unity) ohne Fehler/Warnungen; Regelkern 44/44.
- Headless-Regression: Kombos, schwerer/aufgeladener Angriff, Block/Parry/Dodge, Klassen-Kit (Krieger), Dämonenkampf,
  Truhe/Schmieden/Reparatur – Ergebnisse wie vor der Änderung.
- Koop (Listen-Server + Client): identischer Slice auf beiden (50 Akteure / 4 522 Instanzen), `DBVisualSlice` und
  `DBTimeOfDay` repliziert, Kampf auf dem Client normal, Audit 0 fehlende Referenzen.
- Gerendert: Hof, Dorf, Wald, Brücke, Schrein, Dungeon; Tag/Dämmerung/Nacht/Dämonennacht; Charaktererstellung.
- Gefunden und behoben: Boden-Sonde traf die Spielerkapsel bzw. übersah den (WorldDynamic-)Boden; Palast stand im
  Spawn-Bereich der Test-Dämonen → Hof neu angeordnet; Nacht zu hell, Verderbnis zu flächig → Belichtung/Adern
  angepasst; Foliage-Material kompilierte nicht (Wind-Eingang).
- Bekannt (vorher schon so): Mit `-DBDevSlice` bleibt ein 8 m vor dem Start gespawnter Dämon am König hängen
  (direkte KI-Steuerung ohne NavMesh, Phase 6).

## Nächste Schritte

1. Poly-Haven-Texturen → Instanzen `UseTextures` (größter sichtbarer Sprung, ca. 30 Minuten).
2. Fab-Free-Assets + CC0-Bäume → Kit-Module und Scatter-Einträge.
3. Game Animation Sample → AnimBP auf `UDBAnimInstance`, Set `AS_Player` statt Mannequin-DEV-Set.
4. MetaHumans → Profile `CV_Player_TypeA/B`, `CV_NPC_King`, Crowd.
5. Danach Abnahme nach 19_VISUAL_ACCEPTANCE_CHECKLIST.md und erst dann Phase 6 (Open World, NavMesh, Landscape).
