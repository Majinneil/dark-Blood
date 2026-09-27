# Klassensystem

**Status:** Datenmodell, Stat-Wachstum und Skilltree-Regeln ✅/🟡. Klasseninhalte (Fähigkeiten) folgen in Phase 4.

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
