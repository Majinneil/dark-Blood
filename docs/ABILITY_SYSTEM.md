# Fähigkeitensystem (GAS)

**Status:** Grundgerüst 🟡 (nicht kompiliert). Konkrete Fähigkeiten ab Phase 2/4.

## Komponenten

| Klasse | Aufgabe |
|---|---|
| `UDBAbilitySystemComponent` | am PlayerState (Mixed Replication). Input-Tag → Spec (`GetDynamicSpecSourceTags`), Pressed/Held/Released-Verarbeitung pro Frame, Replikation von Input-Events |
| `UDBGameplayAbility` | Basis: InstancedPerActor, LocalPredicted, blockiert durch `State.Dead`, Aktivierungsregel `OnInputTriggered` / `WhileInputActive` / `OnSpawn` |
| `UDBAbilitySet` | Paket aus Fähigkeiten (+Input-Tag) und Effekten; Handles zum Entfernen (Waffenwechsel) |
| `UDBAttributeSet` | Leben, Ausdauer, Mana (+Max), Regeneration, Angriffs-/Zauberkraft, Krit, Rüstung, Meta `IncomingDamage` |
| `UDBDamageExecution` | Schadensberechnung über den Regelkern |
| `UDBRegenerationEffect` | periodische Regeneration aus Attributen |

Attribut-Basiswerte setzt `UDBProgressionComponent::RecalculateAttributes` aus Klasse + Level (+ Ausrüstung ab
Phase 5) über den Regelkern. Gameplay Effects modifizieren darauf aufbauend (Buffs, Nahrung, Debuffs).

## Input

Enhanced Input → `UDBInputConfig` (Asset oder Code-Fallback): native Aktionen (Bewegen, Kamera, Springen) direkt,
Fähigkeitsaktionen als Tags (`Input.LightAttack`, `Input.Dodge`, `Input.Ability.1` …) an den ASC.

Entwicklungsbelegung: WASD, Maus, Leertaste Springen, Shift Sprint, Alt Ausweichen, LMB leichter Angriff,
F schwerer Angriff, RMB Blocken, E Interagieren, MMB Lock-On, 1–4 Fähigkeiten; Gamepad entsprechend.

## Beispiel: Schattenmal (Phase 4, Entwurf)

- *Werfen:* Projektil-Ability setzt Markierung (Actor oder Gegner-Tag `Mark.Shadow`).
- *Teleport:* zweite Aktivierung → Server prüft Ziel (Navigierbarkeit, Sicht, Reichweite), Motion Warping
  zum Ziel, kurze i-Frames.
- *Upgrades (Skilltree-Ränge/Knoten):* Reichweite, mehrere Markierungen, Cooldown, Auftauchschaden,
  Gegner markieren, Combo nach Teleport, Kettenteleport.
- Eigene Umsetzung und Animationen, keine Kopie fremder Figuren.

Weitere Kernfähigkeiten (Phase 4): Magierflug (Mana-Verbrauch, eigener Movement-Modus), Schutzkreis
(50 m, Stabilität gegen starke Dämonen, Vasallen durchbrechen), Mönch-Konter/Luftkampf, Krieger-Haltungen.
