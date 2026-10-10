# Visual-Pipeline

**Status:** Phase 5.5 Visual Foundation umgesetzt (siehe VISUAL_FOUNDATION.md / VISUAL_FOUNDATION_STATUS.md):
Profile, Animations-Sets, Materialsystem, modularer Baukasten, Vegetation, Licht, Visual Slice. Finale Assets fehlen
noch; DEV-Stand sind das Engine-Mannequin (lokal per Skript) und prozedurale Bauteile.

## Art Direction

Dark Fantasy × mittelalterliches Japan (~14. Jh., fiktiv) × dämonische Fantasy × filmisches Action-RPG.
Realistisch/semi-realistisch, menschliche Proportionen, normale Architektur, realistische Waffen.
**Nicht** blockartig oder voxelartig.

## Rendering (DefaultEngine.ini)

- Lumen GI + Reflexionen, Virtual Shadow Maps, Nanite, Mesh Distance Fields, keine statische Beleuchtung
- DX12 unter Windows
- Skalierungspresets NIEDRIG/MITTEL/HOCH/ULTRA = Engine-Scalability 0–3 (`DefaultScalability.ini`)
- Upscaling (TSR standardmäßig; DLSS/FSR/XeSS optional später, abhängig von Lizenz)

## Austauschbarkeit (Pflicht)

- Gameplay-Klassen referenzieren Visuals nur über weiche Referenzen, Visual-Profile oder Blueprint-Unterklassen.
- `ADBCharacterBase::PlaceholderBody` wird automatisch ausgeblendet, sobald ein Skeletal Mesh gesetzt ist
  (`UDBCharacterVisualComponent` + `UDBCharacterVisualDefinition`).
- Items: `Icon`, `WorldMesh`, `EquippedMesh` als Soft-Refs; `bUsesPlaceholderVisuals` markiert Platzhalter.
- Bosse: Logik ohne Mesh-Kenntnis (siehe BOSS_FRAMEWORK.md).

## Charaktere & Animation (Plan)

- Gemeinsames Skelett für Spielerkörper A/B; Morph Targets für Gesicht/Körper (`FDBAppearance::Morphs`),
  Groom-Haare, Material-Parameter für Haut/Augen/Haarfarbe.
- Animation Blueprint mit Linked Anim Layers pro Waffe (Katana, Nodachi, Speer, Kunai, Magie, Faust),
  Control Rig für IK (Füße, Hände), Motion Warping für Angriffe/Finisher, Root Motion für Ausweichen/Angriffe.
- Montages mit Notify-Fenstern für Combos, Parry, i-Frames.

## Phase 17

Vollständiger Grafik-Pass erst bei stabilem Kernspiel: Modelle, Gesichter, Haare, Kleidung (Chaos Cloth),
Rüstungen, Waffen, Dämonen, Vasallen, Umgebung, Vegetation, Wasser, Wetter, Nebel, Licht, Color Grading, UI, VFX,
Cinematics.

### Stand Phase 17 (2026-10-08)

- **Kampf-Effekte** (`DBCombatFeedback`): Treffer, schwere Treffer, Dämonenklauen, Block, Parade, Haltungsbruch,
  Tod von Dämonen und Bossen. Der Server löst GameplayCues aus (`DBDamageExecution` mit Trefferort und Schaden),
  `ADBCharacterBase` empfängt sie über `IGameplayCueInterface` auf jedem Rechner und spielt Paragon-Partikel
  (Cascade, lokal unter `Content/Paragon*`, nicht im Git – fehlen sie, passiert nichts). Testbefehl `DBFxShow`.
- **Boss-Warnzonen**: durchscheinende Fläche plus Kern, der sich bis zum Einschlag füllt; Schockwelle beim Treffer
  (statt einer grellen Lichtscheibe).
- **Portale und Schreine**: durchscheinende Schleier (Ausgang warm, Eingang blutrot, Abgrund violett, Halle der Echos
  geisterblau); Rast-Schreine als Steinlaterne aus der Modellbibliothek statt Platzhalter-Würfel.
- **Abgrund**: kalter Fels, Blutnaht zwischen Wand und Boden statt Lava auf allen Flächen.
- **Landschaft**: Fels-Schicht triplanar mit zwei Maßstäben (`db_create_realm_landscape_material.py`) – Steilhänge
  zeigen geschichtetes Gestein statt gestreckter Schlieren; das Skript ist jetzt wiederholt ausführbar.
- **Wasserfälle** (`ADBWaterfall`, Material `M_DB_Waterfall` mit Schaum-Fließtextur): Orte werden aus dem Gelände
  gesucht (Klippen ab 56°, Fuß im Meer oder auf ebenem Boden; 6 Stück, 0,15 s), in DAS ENDE und der Dämonenöde als
  Blutfälle; Gischt-Partikel nur in Kameranähe. Testbefehl `DBWaterfall [Index]`.
- **Koop-Korrektur**: Die Paradies-Insel war nicht repliziert und wurde nur auf dem Server gebaut – Clients standen
  dort ohne Boden. Insel und Wasserfälle baut jetzt jeder Rechner selbst.
- Gemessen (offscreen 1600 × 900, kein anderes Spiel aktiv): 78–92 FPS.
- Offen: echte Klippen-Meshes statt geglätteter Rauschhänge (die Wasserfälle wirken dadurch eher dezent), Wetter,
  eigene VFX statt Paragon, Charaktere (MetaHumans, am Ende).
