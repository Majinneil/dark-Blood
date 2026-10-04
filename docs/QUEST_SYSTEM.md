# Questsystem

**Status:** Regeln ✅ (getestet). UE-Anbindung ✅ (headless + Koop getestet). Dialoge und Questgeber-NPCs ✅ (Phase 3). Questmarker auf der Karte folgen mit der Hauptwelt (Phase 6).

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

## Dialoge (Phase 3)

| Teil | Ort |
|---|---|
| Regeln | `DarkBlood::Rules` `Dialogue.h` – Einstieg, Optionen, Effekte, Validierung |
| Definition | `UDBDialogueDefinition` (Asset-Typ `DBDialogue`, `/Game/DarkBlood/Data/Dialogues`) |
| Laufzeit | `UDBDialogueComponent` am PlayerController (Server führt, Client zeigt an) |
| NPC | `ADBNpcCharacter` (`NpcId`, `DialogueId`), Interaktion mit `[E]` |
| UI | `SDBDialogueWidget` (Optionen per Klick oder 1–4, `[E]` weiter) |

- **Einträge** werden von oben nach unten geprüft; der erste, dessen Bedingungen gelten, eröffnet das Gespräch
  (z. B. „Quest abgabebereit“ vor „Quest aktiv“ vor „Begrüßung“).
- **Bedingungen:** Story-Flag vorhanden/fehlt, Queststatus ist/ist nicht (inkl. `Inactive` = nie gestartet).
- **Effekte:** `SetStoryFlag`, `StartQuest`, `TurnInQuest`, `ReportTalk` (Talk-Ziel für Quests). Knoten-Effekte
  (`OnEnter`) laufen beim Betreten, Options-Effekte beim Wählen – in dieser Reihenfolge.
- Der Client sendet nur den Index der angezeigten Option; der Server berechnet die angebotenen Optionen neu und
  lehnt alles andere ab. Gespräche enden, wenn der Spieler sich mehr als 7 m entfernt.

## Story-Slice (DEV)

`MQ01_KingsSummons` → König Aoki, 3 Übungspuppen, Abgabe beim König (250 XP, 100 Mon, 1 Fähigkeitspunkt, kleine
Tasche) → startet `MQ02_EastGate`: Hauptmann Kenji am Osttor (Briefing setzt `Story.CaptainBriefed`, dann erscheinen
3 niedere Dämonen), Abgabe beim Hauptmann (400 XP, 150 Mon, 1 Fähigkeitspunkt, Abenteurertasche,
`Story.EastGateCleared`). Aufbau mit `-DBDevSlice` oder `DBSetupSlice`.
