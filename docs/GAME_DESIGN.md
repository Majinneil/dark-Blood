# Game Design

Verbindliche Quelle für die Vision ist der Masterplan. Dieses Dokument hält die daraus abgeleiteten
Designentscheidungen und Zahlen fest, die im Code verwendet werden.

## Säulen

1. **Eine riesige, nahtlose Welt**, frei bereisbar. Progression entsteht über Gefahr, nicht über Sperren.
2. **Responsives Action-Combat** mit Parry, Dodge, Lock-On und klassenspezifischen Mechaniken.
3. **Lebendige Welt**, die sich auch ohne Spieler entwickelt (Siedlungen, Befreiung, Wiederaufbau).
4. **Koop von Anfang an**: bis zu 4 Spieler, jeder mit eigenem Charakter und gemeinsamem Weltfortschritt.
5. **Keine Frustmechaniken**: Items bleiben beim Tod erhalten, Crafting ohne Auswendiglernen.

## Zahlen (Stand Phase 1, im Regelkern hinterlegt)

| Bereich | Wert | Ort |
|---|---|---|
| Maximallevel | 100 | `FXpCurve::MaxLevel` |
| XP für Level n→n+1 | `round(120 · n^1.65)` | `FXpCurve` |
| Skillpunkte | 1 alle 5 Level + Bosse/wichtige Quests | `FProgressionRules` |
| Stärke (Power Rating) | `round(0.75 · Level + 0.25 · GearScore)` | `ComputePowerRating` |
| Gefahrenstufen | Gering ≥ Max+10 · Angemessen ≥ Min · Herausfordernd ≥ Min−5 · Gefährlich ≥ Min−15 · sonst Extrem | `AssessDanger` |
| Grundbeutel | 20 Plätze | `UDBGameSettings::BasePouchCapacity` |
| Taschen | +9 / +18 / +27, Spezialtaschen 12 | Entwicklungsdaten |
| Todesstrafe | 5 % Mon (max. 500), −5 % Haltbarkeit der Ausrüstung, **kein** Itemverlust | `FDeathPenaltyRules` |
| Tag | 48 Echtminuten, Nacht 19:30–05:30 | `UDBGameSettings`, `FWorldClockRules` |
| Währung | Mon | `FCharacterRecord::Currency` |

## Primärattribute

Kraft, Geschick, Intellekt, Geist, Vitalität, Ausdauer → abgeleitet: Leben, Ausdauer(-punkte), Mana,
Regeneration, Angriffskraft, Zauberkraft, Kritchance (`ComputeDerivedStats`). Klassen definieren Grundwerte und
Wachstum pro Level (`UDBClassDefinition`).

## Tod

Respawn am letzten Ruhepunkt (`RespawnPointId` → `APlayerStart` mit gleichem Tag), sonst Standard-Spawn.
Nachlieferungen: Belohnungen, die nicht ins Inventar passen, werden aufbewahrt und automatisch zugestellt,
sobald Platz frei wird. Es geht nie etwas verloren.

## Seltenheiten

gewöhnlich · ungewöhnlich · selten · episch · legendär · dämonisch. Story- und Bossbelohnungen sind
deterministisch (Quest-Belohnungslisten), nicht ausschließlich RNG.
