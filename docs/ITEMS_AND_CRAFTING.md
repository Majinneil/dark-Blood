# Items, Beute und Crafting

**Status:** Phase 5 ✅ Gameplay – Ausrüstungswerte, Resistenzen, Verbrauchsgüter, Beutetabellen, Crafting,
Reparatur, Inventar- und Schmiede-UI; headless getestet. Item-Icons/Meshes fehlen (Assets).

## Regeln (Regelkern, getestet)

| Bereich | Ort | Kern |
|---|---|---|
| Ausrüstungswerte | `FItemStats`, `ComputeEquipmentStats` | Angriffs-/Zauberkraft, Rüstung, Leben/Ausdauer/Mana, Krit, Resistenzen je Schadensart; **kaputte Items (Haltbarkeit 0) zählen nicht** |
| Verbrauchsgüter | `FConsumableEffect`, `ConsumeItem` | Heilung, Ausdauer, Mana; entfernt genau ein Stück |
| Beute | `FLootTable`, `RollLoot`, `FLootRandom` | garantierte Drops + gewichtete Würfe (inkl. „nichts“), Mon-Spanne, deterministisch bei gleichem Seed |
| Crafting | `FRecipe`, `Craft` | Station, Stufe, Zutaten, Mon; **alles oder nichts** (volle Taschen: nichts wird verbraucht); Items mit Haltbarkeit werden Einzelstücke |
| Reparatur | `RepairCost`, `RepairEquipment` | fehlender Anteil × max(10, 25 % Wert), billigste zuerst, so weit das Geld reicht |

## Unreal-Anbindung

- `UDBItemDefinition`: `Stats` (inkl. Resistenzen), `HealAmount`/`StaminaAmount`/`ManaAmount`, `BaseValue`.
- Attribute: `Resist{Fire,Frost,Lightning,Shadow,Poison,Spirit,DarkBlood}` (vom Schadens-Execution gelesen);
  `UDBProgressionComponent::RecalculateAttributes` addiert die Ausrüstung (bei An-/Ablegen, Tod, Reparatur).
- `UDBInventoryComponent`: `RequestUseItem`, `RequestCraft(Rezept, Station)`, `RequestRepairAll(Station)`,
  `GrantLootTable`/`GrantLoot` (Server; volle Taschen → Nachlieferung, nichts geht verloren).
- Datenassets: `UDBRecipeDefinition` (`DBRecipe`, `/Game/DarkBlood/Data/Recipes`),
  `UDBLootTableDefinition` (`DBLootTable`, `/Game/DarkBlood/Data/Loot`); Beutetabellen werden beim Start gegen den
  Itemkatalog geprüft.
- Welt: `ADBCraftingStation` (z. B. Schmiede, `StationId = Forge`), `ADBLootChest` (einmal pro Charakter).
- **Beute im Koop:** persönlich – jeder Spieler in 60 m würfelt selbst (`ADBEnemyCharacter::LootTableId`), kein
  Wegschnappen.
- UI: Inventar `[I]` (Ausrüstung, Werte, Beutel mit Benutzen/Anlegen/Tasche anlegen), Schmiede-Fenster
  (Rezepte mit Vorrat/Bedarf, Herstellen, „Alles reparieren“); schließt sich beim Weggehen.
- Neue Charaktere starten mit angelegter Klassenwaffe.

## Entwicklungsinhalte

- Geschmiedete Klassenwaffen (Stufe 5): Tamahagane-Katana, Schattenstahl-Kunai, Glutstab, Eisenbandagen.
- Katana-Sammlung (Krieger, alle an der Schmiede): jede Klinge mit eigenen Werten und Treffer-Effekten
  (`UDBItemDefinition::WeaponEffects`, siehe unten).

| Katana | Stufe | Seltenheit | Stärken | Effekte pro Treffer |
|---|---|---|---|---|
| Homura – Flammenklinge | 6 | Selten | Angriff 16, Feuerres. 5 % | +20 % Feuer; 30 %: Brand 4/s, 4 s |
| Yukiore – Frostbiss | 6 | Selten | Angriff 15, Frostres. 10 % | +20 % Frost; +50 % Haltungsschaden |
| Dokuga – Giftzahn | 8 | Selten | Angriff 17, Giftres. 10 % | 50 %: Gift 5/s, 6 s |
| Raikiri – Donnerschneider | 10 | Episch | Angriff 20, Krit 8 % | 25 %: +60 % Blitz; 35 %: Kettenblitz auf nächsten Gegner (6 m, 40 %) |
| Chishio – Blutdurst | 10 | Episch | Angriff 21, +20 Leben | 8 % Lebensraub; 40 %: Blutung 4/s, 5 s |
| Kagekiri – Schattenschnitt | 12 | Episch | Angriff 22, Krit 12 % | +30 % Schatten |
| Reiha – Geisterklinge | 12 | Episch | Angriff 19, +20 Ausdauer, Geistres. 15 % | +35 % Geist |
| Kegare – Verdorbenes Katana | 15 | Dämonisch | Angriff 30, Krit 5 %, Dunkelblutres. −10 % | +40 % Dunkelblut, 12 % Lebensraub; 20 %: Verderbnis 6/s, 5 s |

**Waffeneffekte** (`Combat/DBWeaponEffects`, nur Server, nur Nahkampftreffer des Trägers): Elementarschaden,
Schaden über Zeit (`UDBDamageOverTimeEffect`, 1 Tick/s, stapelt nicht: `State.Burning/Poisoned/Bleeding/Afflicted`,
endet mit dem Tod des Ziels), Lebensraub, Haltungsbruch, Kettenschlag. Alles läuft durch die normale
Schadensberechnung (Resistenzen, Rüstung, Blocken, Unverwundbarkeit); ausgewichene oder parierte Treffer lösen nichts aus.
Die Klinge erscheint in der rechten Hand (`UDBCharacterVisualComponent::SetWeaponItem`, Griffposition aus der
Referenzpose der Hand, im Koop aus der replizierten Ausrüstung); ohne importiertes Fab-Modell eine schlichte Stahlklinge.
- Rüstung: Ashigaru-Helm/-Brustpanzer, Lederne Kote, Eisen-Suneate, Waraji; Schutzamulett (Geist/Schatten).
- Verbrauch: Onigiri (40 Leben, 30 Ausdauer), Heiltrank (120), Geistertee (60 Mana).
- Material: Tamahagane, Dämonenerz, Dämonenhorn, Leder, Geisterpapier.
- Beute: `LT_LesserDemon` (garantiert Dämonenhorn, 2 Würfe, 5–15 Mon), `LT_TrainingDummy`, `LT_Chest_Courtyard`.
- Story-Slice: Schmiede und Truhe im Burghof.

## Test-Kommandos

`DBUse <ItemId>`, `DBUseItem <Sektion> <Index>`, `DBEquipById <ItemId>`, `DBCraft <RezeptId>`, `DBRepair`,
`DBGrantLoot <Tabelle>`, `DBGoto Forge|Chest`.

## Offen

Händler/Handel (Phase 7, Siedlungen), Item-Icons und Meshes, Seltenheits-Affixe, Taschen-Drag-and-Drop,
Rezept-Freischaltung über Bücher/Quests.
