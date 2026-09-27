# Visual-Pipeline

**Status:** Rendering-Grundkonfiguration gesetzt. Keine finalen Assets vorhanden. Alle Visuals im Spiel sind
Entwicklungsplatzhalter (Engine-Zylinder als Körper, Text-Namensschilder, Canvas-Overlay).

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
- `ADBCharacterBase::PlaceholderBody` wird automatisch ausgeblendet, sobald ein Skeletal Mesh gesetzt ist.
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
