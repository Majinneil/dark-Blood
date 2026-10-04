# Multiplayer

**Status:** Architektur und Replikation implementiert, **nicht getestet** (kein UE-Build möglich).

## Modell

- Bis zu 4 Spieler (`GameSession.MaxPlayers=4`), Listen Server oder Dedicated Server (`DarkBloodServer`-Target).
- Server ist die einzige Autorität für Schaden, Währung, Loot, Quests, Skillpunkte, Bossstatus und Charakterwerte.
- Jeder Spieler hat eigenen Charakter, Namen, Klasse, Level, Skills, Ausrüstung, Inventar und Taschen
  (`ADBPlayerState` + Komponenten). Welt und Story sind gemeinsam (`ADBGameState`).

## Persistenzmodi (`UDBGameSettings::PersistenceMode`, `-DBPersistence=`)

| Modus | Einsatz | Ablauf |
|---|---|---|
| `LocalCharacters` (Standard) | Singleplayer, Koop per Listen Server | Jeder Spieler besitzt seine Charaktere lokal. Beim Beitritt lädt der Client den Datensatz hoch (`ServerUploadCharacter`, max. 64 KB). Der Server prüft Prüfsumme, Version, Name, Klasse, Progressions-Konsistenz, Item-IDs, Stapelgrößen, Instanz-Duplikate, Equipment-Regeln und Skillpunkte, bevor der Charakter übernommen wird. Autosaves schickt der Server als Snapshot zurück (`ClientStoreCharacterSnapshot`). |
| `ServerAuthoritative` | Dedicated Server | Der Server speichert Charaktere unter einem Hash der Plattform-ID (`MakeServerSlotKey`). Fehlt ein Charakter, fordert der Server die Erstellung an; Name, Klasse und Aussehen werden serverseitig validiert. |

**Vertrauensgrenze im Koop-Modus:** Ein lokal gespeicherter Charakter kann offline manipuliert werden, bleibt aber
innerhalb der Regeln (plausibles Level/XP, gültige Items). Das ist für Koop mit Freunden das übliche Modell.
Für öffentliche Server wird der Modus `ServerAuthoritative` verwendet.

**Duplikationsschutz:** Derselbe Charakter (CharacterId) kann nicht zweimal in einer Sitzung sein. Unique-Item-Instanzen
werden beim Upload auf Duplikate geprüft. Ein zweiter Upload pro Verbindung wird abgelehnt.

## Replikation

| Daten | Ort | Bedingung |
|---|---|---|
| Profil (Charaktername, Klasse, Aussehen) | PlayerState | alle |
| Leben/Ausdauer/Mana (+Max) | AttributeSet (ASC am PlayerState, Mixed Mode) | alle |
| Sekundärwerte (Regen, Angriffs-/Zauberkraft, Krit, Rüstung) | AttributeSet | nur Besitzer |
| Level, Gear-Score | Progression | alle (Gruppenanzeige) |
| XP, Skillpunkte, Skill-Ränge | Progression | nur Besitzer |
| Inventar | Fast Array (Delta) | nur Besitzer |
| Ausgerüstete Items | Array | alle (Visuals) |
| Währung, Nachlieferungen | Inventory | nur Besitzer |
| Aktuelle Region | PlayerState | alle (Karte/Gruppe) |
| Entdeckte Regionen | PlayerState | nur Besitzer |
| Weltuhr | WorldState | alle, alle 2 s, Clients extrapolieren |
| Regionen (Einfluss als Byte), Story-Flags, Vasallen | WorldState | alle |
| Quests | Quest-Komponenten | alle |

Client-Anfragen: `Request*`-Funktionen der Komponenten → `Server*`-RPCs → Regelkern → Sicht-Sync oder
`ClientRequestFailed`.

## Koop-Regeln (Stand Phase 1)

- Gemeinsame Quests (Story): Fortschritt gemeinsam, Belohnung für jeden Spieler der Sitzung.
- Persönliche Quests: `Kill`/`Custom`-Ereignisse zählen für alle Spieler (Gruppenerfolg), `Collect`/`Talk`/
  `Reach`/`Interact` nur für den Auslöser. Ein Party-System mit Reichweite folgt in Phase 3.
- Bossskalierung erfolgt über Mechaniken, nicht nur über HP (siehe BOSS_FRAMEWORK.md).

## Testplan (Phase 19, vorgezogen sobald UE-Build steht)

2/3/4 Spieler, Reconnect, Disconnect während Autosave, Dedicated Server, Loot/Quest/Boss/Settlement-Sync,
Tod/Respawn, Taschenwechsel unter Latenz (`Net PktLag=150`, `Net PktLoss=2`).
