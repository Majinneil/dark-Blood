# Multiplayer

**Status:** Implementiert und getestet (Phase 19, 2026-10-11): bis 4 Spieler, Listen- und Dedicated-Server, unter
simulierter Latenz – siehe „Testprotokoll“ unten.

## Modell

- Bis zu 4 Spieler (`GameSession.MaxPlayers=4`), Listen Server oder Dedicated Server (`DarkBloodServer`-Target).
- Server ist die einzige Autorität für Schaden, Währung, Loot, Quests, Skillpunkte, Bossstatus und Charakterwerte.
- Jeder Spieler hat eigenen Charakter, Namen, Klasse, Level, Skills, Ausrüstung, Inventar und Taschen
  (`ADBPlayerState` + Komponenten). Welt und Story sind gemeinsam (`ADBGameState`).

## Persistenzmodi (`UDBGameSettings::PersistenceMode`, `-DBPersistence=`)

| Modus | Einsatz | Ablauf |
|---|---|---|
| `LocalCharacters` (Standard) | Singleplayer, Koop per Listen Server | Jeder Spieler besitzt seine Charaktere lokal. Beim Beitritt lädt der Client den Datensatz hoch (`ServerUploadCharacter`, max. 64 KB). Der Server prüft Prüfsumme, Version, Name, Klasse, Progressions-Konsistenz, Item-IDs, Stapelgrößen, Instanz-Duplikate, Equipment-Regeln und Skillpunkte, bevor der Charakter übernommen wird. Autosaves schickt der Server als Snapshot zurück (`ClientStoreCharacterSnapshot`). |
| `ServerAuthoritative` (`-DBPersistence=Server`) | Dedicated Server | Der Server speichert Charaktere unter einem Hash der Plattform-ID (`MakeServerSlotKey`). Ohne Online-Plattform (NULL-Subsystem, LAN/Direkt-IP) wechselt diese ID bei jedem Start – dann gilt der dauerhafte Spieler-Schlüssel, den jeder Client beim Login mitschickt (`UDBLocalPlayer`, `?DBKey=`, einmal pro Rechner unter `Saved/SaveGames/DB_PlayerKey.txt` erzeugt). Fehlt ein Charakter, fordert der Server die Erstellung an; Name, Klasse und Aussehen werden serverseitig validiert. |

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

## Koop-Regeln

- Gemeinsame Quests (Story): Fortschritt gemeinsam, Belohnung für jeden Spieler der Sitzung.
- Persönliche Quests: ein Kill zählt für alle Spieler im Umkreis von 150 m um den, der ihn landete (wer zusammen kämpft,
  kommt gemeinsam voran; ein Freund am anderen Ende der Welt nicht); ohne bekannten Täter (Falle) für alle.
  `Custom` zählt für alle, `Collect`/`Talk`/`Reach`/`Interact` nur für den Auslöser.
- Beute ist persönlich: Jeder Spieler im Umkreis von 60 m würfelt seine eigene (kein Wegschnappen).
- Ankunft mehrerer Spieler am selben Ort (Dungeon, Abgrund, Halle, Tore, Kutsche): `DBTeleport::MovePawn` verteilt sie
  auf freie Plätze (ein Engine-Teleport auf einen besetzten Punkt schlägt sonst still fehl).
- Doppelter Charakter: abgelehnt mit Grund auf dem Bildschirm, der Server trennt nach 8 s.
- Bossskalierung erfolgt über Mechaniken, nicht nur über HP (siehe BOSS_FRAMEWORK.md).

## Testprotokoll (Phase 19, 2026-10-11)

Skript `run-mp.ps1` (Server + bis zu 4 Clients, verdeckt, headless), Latenz `NetEmulation.PktLag 150`,
`NetEmulation.PktLoss 2`:

- **4 Spieler unter Latenz:** alle im Abgrund, Kampf gegen Dämonen und Akakage – Treffer, Umwerfen, Haltungsbruch der
  Clients serverseitig bestätigt; Beute je Kill für jeden Spieler in Reichweite; Tod und Respawn eines Clients;
  Tasche unter Latenz angelegt. **Behoben:** der vierte Spieler am selben Ankunftspunkt wurde nicht teleportiert.
- **Doppelter Charakter:** zweiter Login desselben Charakters abgelehnt („Dieser Charakter ist bereits in der
  Sitzung.“). **Ergänzt:** Grund im HUD, Server trennt nach 8 s.
- **Trennen während des Speicherns, Wiederverbinden:** Client trennt, während der Server zweimal speichert; derselbe
  Charakter verbindet neu mit Münzen und Tasche.
- **Dedicated Server** (`UnrealEditor-Cmd -server`): 2 Clients unter 120 ms Latenz, Abgrund, Bosskampf, Beute.
  Serverseitige Speicherung über zwei Server-Starts: derselbe Spieler erhält seinen Charakter zurück (827 Mon).
  **Behoben:** ohne Plattform-ID ging der Charakter bei jedem Client-Start verloren (Spieler-Schlüssel).
- **Gruppen-Fortschritt:** persönliche Quest `SQ_DemonHunt` – Täter und Mitspieler nebenan erledigen sie, ein Spieler
  in der Hauptstadt bleibt bei 0.
- Frühere Phasen: Koop-Tests je System (Kampf, Dialog, Quests, Dungeons, Bosse, Paradies, Endgame, Audio-Cues).
