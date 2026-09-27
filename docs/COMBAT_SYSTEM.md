# Kampfsystem

**Status:** Phase 2 (Vertical Slice) ✅ Gameplay – spielbar mit Platzhalterkörpern, headless und im Koop getestet. Präsentation (Animationen, VFX, Audio) fehlt: braucht Assets; die Anschlüsse (Montage-Felder, GameplayCues) sind vorhanden.

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

## Phase 2 – umgesetzt

Timing kommt aus Daten (Windup → Treffer → Recovery), nicht aus Animationen; Montages sind optional
(`FDBAttackStepConfig::Montage`, `GuardMontage`, …). Treffer werden nur auf dem Server ermittelt
(Kugel-Overlap + Bogenprüfung `Rules::IsInsideAttackArc`) und über `UDBDamageEffect` angewendet.

| Aktion | Klasse | Werte (DEV) |
|---|---|---|
| Leichte Combo (3 Schläge) | `UDBAbility_LightCombo` | 10/12/18 Schaden, 8/9/14 Ausdauer, Combo-Fenster 0,9 s nach dem Treffer |
| Schwerer / aufgeladener Angriff | `UDBAbility_HeavyAttack` | 28 Schaden, halten 0,35–1,5 s → bis ×2,5 Schaden, ×3 Poise, voll geladen = Knockdown |
| Block / Perfect Parry / Konter | `UDBAbility_Block` | Parierfenster 0,15 s; Parade → Angreifer `ParriedStagger`, Verteidiger 1 s Konterfenster (×2 Schaden, ×3 Poise); Angriff aus dem Block senkt die Deckung |
| Ausweichen | `UDBAbility_Dodge` | 450 cm in 0,4 s (Eingaberichtung, sonst Rückschritt), 0,3 s i-Frames, 20 Ausdauer |
| Sprint | `UDBAbility_Sprint` | ×1,6 Tempo, 12 Ausdauer/s in Bewegung |
| Luftangriff | `UDBAbility_LightCombo` (Kontext *Air*) | in der Luft: Sturz nach unten (1800 cm/s), 20 Schaden rundum (180°), Knockdown |
| Sprint-Angriff | Kontext *Sprint* | aus dem Sprint: Ausfallschritt 350 cm, 22 Schaden, 28 Poise |
| Dash-Angriff | Kontext *Dash* | bis 0,35 s nach dem Ausweichen: schneller Stoß 200 cm, 16 Schaden |
| Doppelsprung | `UDBAbility_DoubleJump` (passiv) | `Movement.DoubleJump` (repliziertes Tag) → `JumpMaxCount = 2`; DEV-Moveset schaltet ihn frei, später Skilltree |
| Aufstehen | `UDBAbility_HitReact` | nach Knockdown 0,4 s unverwundbar (kein Dauer-Knockdown) |
| Trefferreaktion | `UDBAbility_HitReact` | Event `Event.Combat.HitReact`; Stagger 0,6 s, Knockdown 1,6 s, Parried 1,0 s + Rückstoß |
| Lock-On | `UDBLockOnComponent` | Kegel 45°, 20 m, Sichtlinie; Strafing; Zielwechsel per Maus-/Stick-Flick; Server erhält nur Hinweis |
| Trainingsgegner | `ADBTrainingDummy` (`ADBEnemyCharacter`) | 60 HP, 30 Poise, 20 XP, `Kill TrainingDummy`, Respawn nach 3 s, telegrafierter Schlag (0,8 s Windup), verankert (kein Rückstoß) |
| Erster Gegner | `ADBLesserDemon` + `UDBMeleeAIComponent` | 140 HP, 45 Poise, 20 Rüstung, Stufe 3, 45 XP, `Kill LesserDemon`; KI: Suchen (15 m, Sichtlinie) → Verfolgen → Klauenhieb (0,5 s Windup) → 1,2–2,4 s Pause; Leine 25 m; Aggro bei Treffer |

**Poise** (`Poise`/`MaxPoise`, Meta `IncomingPoiseDamage`): `Rules::ApplyPoiseDamage` → Flinch / Stagger /
Knockdown; geblockte Treffer 30 % Poise-Schaden, Deckung verhindert Knockdown; Poise füllt sich 3 s nach dem
letzten Poise-Schaden wieder auf. Ausdauer 0 beim Blocken → Guard Break (Stagger).
**Ausdauer:** Aktion startet mit jeder positiven Ausdauer (`Rules::CanStartStaminaAction`); Regeneration pausiert
1 s nach Verbrauch sowie während Sprint und Block (Tag-Bedingungen im `UDBRegenerationEffect`).

**GameplayCues** (Präsentations-Anschlüsse, ohne Notify-Assets wirkungslos): `GameplayCue.Combat.Hit`, `.Blocked`,
`.Parried` (Ziel des Treffers), `.Stagger`, `.Dodge`. Notifies gehören nach `/Game/DarkBlood/GameplayCues`
(`DefaultGame.ini`).

**Offen (braucht Assets bzw. spätere Phasen):** Animationen/Montages und Get-up-Animation, VFX/Audio als
GameplayCue-Notifies, KI mit Navigation/StateTree (aktuelle KI steuert direkt, ohne NavMesh), Client-Vorhersage von
Trefferfeedback, Balancing mit echten Klassen-Kits (Phase 4).

### Test-Kommandos

`DBSpawnDummy [cm]`, `DBSpawnEnemy [cm]`, `DBJump`, `DBDummyAttack`, `DBDummyAutoAttack <s>`, `DBInput <LightAttack|HeavyAttack|Dodge|Block|Sprint> [Halten s]`,
`DBLockOn`, `DBAfter <s> <Kommando>`, `DBDumpCombat`. Beispiel (headless, Karte mit Boden):

Koop-Tests: Server mit `?listen` und `-DBCheats`, Client mit `-DBAutoExec="DBAfter 3 DBSpawnDummy 180|DBAfter 5 DBInput LightAttack|…"`
(Befehle mit `|` getrennt, laufen nach dem Verbinden).

```
UnrealEditor-Cmd.exe DarkBlood.uproject "/Engine/Maps/Templates/Template_Default?game=/Script/DarkBlood.DBGameMode" -game -nullrhi
  -ExecCmds="DBSpawnDummy 180,DBAfter 1 DBInput LightAttack,DBAfter 3 DBDummyAttack,DBAfter 3.7 DBInput Block 0.4,DBAfter 4.1 DBInput LightAttack,DBAfter 6 DBDumpCombat,DBAfter 7 quit"
```

Serverautorität: Schaden, Poise und Reaktionen entstehen ausschließlich auf dem Server. Aktionen sind LocalPredicted
(Ausdauer, Tags, Root Motion), Trefferreaktionen ServerInitiated.
