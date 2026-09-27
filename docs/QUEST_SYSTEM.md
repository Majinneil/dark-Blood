# Questsystem

**Status:** Regeln ✅ (getestet). UE-Anbindung 🟡. Dialoge, Marker und Questgeber-NPCs folgen in Phase 3.

## Bausteine

| Teil | Ort |
|---|---|
| Definition | `UDBQuestDefinition` (Data Asset, Typ `DBQuest`) → `FQuestDefinition` |
| Regeln | `FQuestLog` (Start, Ereignisse, Abgabe, Fehlschlag, Abbruch, Abgleich) |
| Gemeinsamer Log | `UDBQuestComponent` am GameState (Story, `Scope = Shared`) |
| Persönlicher Log | `UDBQuestComponent` am PlayerState (`Scope = Personal`) |
| Routing/Belohnung | `UDBQuestSubsystem` (Server) |

## Definition

- Kategorien: Hauptquest, Nebenquest, Klassenquest, Dungeon, Kopfgeld, Regional, Versteckt, Dynamisch, Siedlung.
- Voraussetzungen: Story-Flags (Welt) und abgeschlossene Quests.
- Ziele: `Kill`, `Collect`, `Reach`, `Talk`, `Interact`, `Custom` mit Ziel-ID und Anzahl, optional.
- `bSequential`: nur das erste offene Pflichtziel zählt.
- `bAutoComplete`: sonst Status `ReadyToTurnIn` bis zur Abgabe beim Questgeber.
- Hauptquests sind nie abbrechbar.
- Belohnung: XP, Mon, Fähigkeitspunkte, Items (deterministisch), Story-Flags.

## Ereignisse melden

Gameplay meldet nur Fakten, der Questcode entscheidet:

```cpp
UDBQuestSubsystem::Get(this)->ReportEvent(EDBObjectiveKind::Kill, TEXT("Oni_Grunt"), 1, KillerPlayerState);
```

Blueprints: Knoten *Report Event* (nur Server). Belohnungen passender Quests werden sofort vergeben;
volle Inventare führen zu Nachlieferungen.

## Chronik des Dunklen Blutes (Plan, Phase 3)

Buch-UI auf Basis der replizierten Questsichten (`GetQuests()`), Regionsentdeckung, Bestiarium (besiegte
Gegner-IDs, Phase 2+), Bosse/Vasallen (Weltstatus), Lore-Einträge (Data Assets, per Entdeckung freigeschaltet).

## Entwicklungsquest

`MQ01_KingsSummons` („Der Ruf des Königs [DEV]“): Mit dem König sprechen, 3 Trainingspuppen besiegen.
Belohnung: 250 XP, 100 Mon, 1 Fähigkeitspunkt, kleine Reisetasche, Flag `Story.KingsSummonsDone`.
Sie startet automatisch in neuen Welten (`InitialSharedQuests`).
