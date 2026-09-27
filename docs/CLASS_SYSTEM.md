# Klassensystem

**Status:** Phase 4 ✅ Gameplay – vier Klassen-Kits, Signaturfähigkeiten, Skilltrees mit mechanischen Knoten, Skilltree-UI; headless getestet. Animationen/VFX fehlen (Assets).

## Datenmodell

`UDBClassDefinition` (Data Asset, Typ `DBClass`):

- `ClassId` (gespeichert, für Item-Beschränkungen), `ClassTag` (`Class.*`), Name, Beschreibung
- Grundwerte Stufe 1 + Wachstum pro Stufe (6 Primärattribute)
- `BaseAbilitySet`: Fähigkeiten/Effekte, die jedes Klassenmitglied hat
- `SkillTree`: Knoten mit Kosten pro Rang, Maximalrang, Mindeststufe, Voraussetzungen, optional gewährter
  Fähigkeit + Input-Tag, UI-Position
- Startausrüstung und Start-Mon

Neue Klassen = neues Asset. Kein Code nötig, außer für neue Mechaniken (Abilities).

## Klassen (Plan)

| Klasse | ID | Fokus | Entwicklungswerte (Stufe 1) |
|---|---|---|---|
| Krieger | `Warrior` | Katana, Nodachi, Naginata/Speer, Blocken, Parieren, schwere Techniken | Kraft 14, Vit 14 |
| Schattenläufer | `Shadowrunner` | Kunai, Tempo, Teleport, Attentate, schnelle Kombos | Geschick 15 |
| Magier | `Mage` | Elementmagie, Fernkampf, Flug, Schutzkreis, Support | Intellekt 16, Geist 12 |
| Mönch | `Monk` | Faust/Tritte, spirituelle Kräfte, Konter, Luftkampf | ausgeglichen, Geist 12 |

## Skilltree-Regeln (implementiert)

- Freischalten nur serverseitig (`ServerUnlockSkill`) über `DarkBlood::Rules::UnlockSkill`.
- Prüfungen: Knoten existiert, Maximalrang, Stufe, alle Voraussetzungen ≥ Rang 1, genug Punkte.
- Gewährte Fähigkeiten werden beim Laden neu vergeben; höhere Ränge erhöhen den Ability-Level.
- Upload-Validierung: ausgegebene + freie Punkte ≤ verdiente Punkte.

## Designregel

Knoten verändern Mechaniken („Schattenmal: mehrere Markierungen“, „Schutzkreis: Heilwirkung“), keine
+2-%-Füllknoten. Beispiele in ABILITY_SYSTEM.md.

## Klassen-Kits (Phase 4, DEV-Werte)

Jede Klasse hat ein eigenes Moveset (leicht/schwer) plus Ausweichen, Blocken, Sprinten und Trefferreaktion.
Signaturfähigkeiten liegen auf **Taste 1/2** und werden über den Skilltree freigeschaltet (`[K]`).

| Klasse | Moveset | Taste 1 | Taste 2 | Passive Knoten |
|---|---|---|---|---|
| Krieger | Katana-Kombo (3), Schwerer Hieb (aufladbar) | **Eiserne Haltung** – an/aus: +40 Rüstung/Poise, Block halbe Ausdauer, −20 % Tempo; R2: Poise-Schaden halbiert | **Durchbruch** – 6-m-Sturmangriff, 30 Schaden + Knockdown; R2: Abklingzeit 4 s | Konterschnitt (Konter ×3/×4), Blutrausch (+3 Ausdauer je Treffer/Rang) |
| Schattenläufer | Kunai-Serie (4), Schwerer Hieb | **Schattenmal** – markieren, erneut: Teleport hinter das Ziel (0,3 s unverwundbar, nächster Treffer +50 %); R2: 15 Schattenschaden bei Ankunft | **Rauchschleier** – 4 s unsichtbar für Gegner-KI, Angriff beendet ihn | Hinterhalt (+50 %/Rang von hinten), Leichtfüßig (Ausweichen halbe Kosten) |
| Magier | Magiegeschoss (Feuer, Projektil), Frostlanze | **Schutzkreis** – 8 m, 10 s: Dämonen hinausgedrängt + 8 Geistschaden/s, Verbündete −30 % Schaden; R2: Heilung 5/s | **Flug** – an/aus, 8 Mana/s, Bewegung folgt der Kamera | Kettenblitz (Geschoss springt auf +1 Gegner/Rang, 60 %), Manafluss (+1,5 Manareg./Rang) |
| Mönch | Faustfolge (5, letzter Tritt rundum), Bergstoß (aufladbar) | **Konterhaltung** – 0,6 s: nächster Treffer wird abgefangen und mit 25 Geistschaden beantwortet; R2: +50 % | **Himmelstritt** – dich und Gegner vor dir in die Luft (danach Luftangriffe) | Innere Ruhe (Parierfenster +0,1 s/Rang), Eisenkörper (+20 Poise/Rang) |

Alle Klassen: **Doppelsprung** (Stufe 2). Kontextangriffe (Luft/Sprint/Dash) haben alle Nahkampf-Movesets.

**Technik:** Mana-Kosten und Abklingzeiten in `UDBGameplayAbility` (`ManaCost`, `CooldownSeconds`, `CooldownTag`;
Abklingzeiten sind vorhergesagte, zeitgesteuerte Tags). Knoten-Ränge liest jede Fähigkeit selbst
(`GetSkillRank(DBSkillNodes::…)`). Passive Knoten sind `UDBAbility_PassiveBonus` (Effekt × Rang, bei Rangaufstieg
neu aktiviert). Projektil `ADBProjectile`, Schutzkreis `ADBWardingCircle`.

**Offen:** Respec (Punkte zurücksetzen), weitere Knoten je Klasse (Nodachi/Speer, Attentate, Elemente, Luftkampf),
Waffenwechsel mit eigenem Moveset (Phase 5), Animationen/VFX.
