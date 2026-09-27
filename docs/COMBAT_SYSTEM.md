# Kampfsystem

**Status:** Schadensregeln (Regelkern) und GAS-Grundlage ✅/🟡. Spielbare Kampfaktionen folgen in Phase 2.

## Schadensmodell (implementiert)

`DarkBlood::Rules::ResolveDamage` ist die einzige Stelle, an der Schaden berechnet wird. Die GAS-Execution
`UDBDamageExecution` sammelt nur Eingaben:

1. Unverwundbar (`State.Invulnerable`, Dodge-i-Frames) → kein Schaden, außer der Angriff ignoriert i-Frames.
2. Perfect Parry (`State.ParryWindow`) → kein Schaden, außer `Damage.Unparryable`.
3. Basis × (1 + Angriffs-/Zauberkraft · 1 %).
4. Kritisch: Wurf < Kritchance → × Kritmultiplikator (Wurf kommt vom Server-RNG).
5. Physisch: Rüstungsminderung `Armor / (Armor + 50 · Angreiferlevel)`, max. 75 %.
   Elementar: Resistenz, begrenzt auf [−100 %, 80 %].
6. Blocken (`State.Blocking`, nicht `Damage.Unblockable`): 70 % Minderung, kostet Ausdauer (0,5 je geblockter
   Schadenspunkt). Rest geht auf Leben.
7. Mindestschaden 1 bei ungeblockten Treffern.

Ergebnis → Meta-Attribut `IncomingDamage` → `UDBAttributeSet::PostGameplayEffectExecute` zieht Leben ab und
meldet bei 0 `OnOutOfHealth` → `ADBCharacterBase::HandleOutOfHealth` → GameMode (Todesstrafe, Respawn).

Schadensarten (Tags): Physical, Fire, Frost, Lightning, Shadow, Poison, Spirit, DarkBlood.

## Ressourcen

- **Ausdauer:** Sprint, schwere Angriffe, Dodge, Block, teilweise Parry, Bewegungsfähigkeiten.
- **Mana:** Magie, Schutzkreis, Flug, bestimmte Fähigkeiten.
- Regeneration über `UDBRegenerationEffect` (periodisch, 0,25 s, liest die *Regen-Attribute*).
  Phase 2: Regenerationsverzögerung nach Anstrengung (Tag `State.Exhausted` als Voraussetzung).

## Geplant für Phase 2 (Vertical Slice)

| Aktion | Umsetzung |
|---|---|
| Light-Combo, Heavy, Charged, Sprint-/Dash-/Jump-/Air-Attack | `UDBGameplayAbility`-Unterklassen + Montages mit Combo-Fenstern (AnimNotifyStates), Treffer per Trace im Notify, Schaden per GE-Spec mit `SetByCaller.Damage` |
| Block / Perfect Parry / Konter | `WhileInputActive`-Ability setzt `State.Blocking`; die ersten ~150 ms zusätzlich `State.ParryWindow`; erfolgreiche Parade → GameplayEvent → Konterfenster |
| Dodge / Roll / Side-/Back-Step / Dash | Root-Motion-Montage + Motion Warping, i-Frames als GE mit `State.Invulnerable`, Ausdauerkosten |
| Doppelsprung | `JumpMaxCount = 2` nach Freischaltung; eigene Animation/VFX/Sound |
| Lock-On | lokale Zielwahl (Kegel + Sichtlinie), Server erhält nur Ziel-Hinweis; Kamera fokussiert, Strafing, Dodge relativ zum Ziel, Zielwechsel per Stick/Maus |
| Trefferreaktion, Knockdown, Get-up | Poise-Wert (Phase 2 Attribut) und Reaktions-Abilities |
| Trainingsgegner | NPC-Charakter mit eigenem ASC, `Kill`-Questereignis `TrainingDummy` |

Serverautorität: Treffer werden vom Server bestätigt (Client-Vorhersage für Animation/Feedback, Server-Trace mit
Toleranz für Latenz). Schaden entsteht ausschließlich auf dem Server.
