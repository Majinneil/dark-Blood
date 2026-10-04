# Content-Pipeline

## Primary Data Assets

| Typ | Klasse | Ordner | ID-Feld |
|---|---|---|---|
| `DBClass` | `UDBClassDefinition` | `/Game/DarkBlood/Data/Classes` | `ClassId` |
| `DBItem` | `UDBItemDefinition` | `/Game/DarkBlood/Data/Items` | `ItemId` |
| `DBQuest` | `UDBQuestDefinition` | `/Game/DarkBlood/Data/Quests` | `QuestId` |
| `DBRegion` | `UDBRegionDefinition` | `/Game/DarkBlood/Data/Regions` | `RegionId` |

IDs sind stabil und werden gespeichert. **Nie umbenennen**, nachdem Saves existieren; stattdessen neue ID und
Migration.

Anlegen im Editor: *Content Browser → Rechtsklick → Miscellaneous → Data Asset → Klasse wählen*, im richtigen
Ordner speichern. Der Asset Manager findet Assets automatisch (`DefaultGame.ini`).

## Entwicklungsdaten

`FDBDevelopmentContent` registriert fehlende Einträge im Code, alle mit `[DEV]` im Namen:

- Klassen: `Warrior`, `Shadowrunner`, `Mage`, `Monk`
- Items: `Katana_Dev`, `Kunai_Dev`, `Staff_Dev`, `Handwraps_Dev`, `Tamahagane`, `DemonOre`, `RiceBall`,
  `KingsSeal` (Quest), `Bag_Small` (+9), `Bag_Adventurer` (+18), `Bag_LargeBackpack` (+27), `Bag_Materials`,
  `Bag_Provisions`, `Bag_Scrolls`, `Bag_Loot`
- Regionen: `Capital`, `Region01`–`Region14`, `TheEnd`, `Paradise`
- Quest: `MQ01_KingsSummons`

Ein Asset mit derselben ID ersetzt den Platzhalter automatisch. Das Debug-Overlay zeigt, solange Platzhalter
aktiv sind.

## Benötigte Editor-Assets für Phase 2/3

1. Input: `IA_Move`, `IA_Look`, `IA_Jump`, `IA_*`-Fähigkeiten, `IMC_Default`, `DA_InputConfig` →
   in `BP_DBPlayerController` (Unterklasse von `ADBPlayerController`) zuweisen.
2. Karte `L_Realm` (World Partition) mit `ADBRegionVolume`s und `PlayerStart`s (Tags = Respawnpunkt-IDs).
3. Charakter-Blueprint `BP_DBPlayerCharacter` mit Skeletal Mesh + AnimBP (Platzhalter erlaubt).
4. Datenassets für Klassen, Items, Regionen, Quests.

Die Projektkonfiguration verweist bis dahin auf Engine-Klassen/-Vorlagen; nach dem Anlegen in
`DefaultEngine.ini` (Karten, GameMode-Blueprint) umstellen.

## Regeln

- Gameplay-Werte nur in Data Assets/Data Tables, nie in Level-Blueprints.
- Visuelle Referenzen immer als `TSoftObjectPtr` (Streaming, austauschbar).
- Platzhalter-Assets liegen unter `/Game/DarkBlood/Dev/` und tragen das Präfix `DEV_`.
- Binärassets über Git LFS (`.gitattributes`).
