# Speichersystem

**Status:** Serialisierung ✅ (getestet). SaveGame-Anbindung 🟡 (nicht kompiliert).

## Aufbau

```
UDBCharacterSaveGame (USaveGame)            UDBWorldSaveGame (USaveGame)
  Kopf: CharacterId, Name, Klasse, Level,     Kopf: WorldId, Zeitpunkt
        Zeitpunkt  (nur für Menüs)            RecordData = SerializeWorld(...)
  RecordData = SerializeCharacter(...)
```

Die eigentlichen Daten sind Regelkern-Datensätze in einem eigenen Binärformat:

```
"DBCH" | u32 Version | u32 Nutzdatenlänge | u32 CRC32(Nutzdaten) | Nutzdaten (little endian)
"DBWD" | ...                                                     (Welt)
```

Vorteile: identische Bytes für lokale Saves, Server-Speicherung, Upload und spätere Datenbank; vollständig ohne
Engine testbar; strikte Längen- und Grenzprüfungen (keine Abstürze bei beschädigten Daten).

## Inhalt

**Charakter (`FCharacterRecord`):** CharacterId (GUID), Name, Klasse, Aussehen (Körper, Gesicht, Haut, Haare,
Augen, Narben, Morphs, Stimme), Progression (Level, XP, Skillpunkte), Skill-Ränge, Währung, Inventar (alle
Sektionen inkl. ausgerüsteter Taschen), Questitems, Equipment, persönliche Quests, entdeckte Regionen, Titel,
Respawnpunkt, Spielzeit, Nachlieferungen.

**Welt (`FWorldRecord`):** WorldId, Uhr, Story-Flags, besiegte Bosse, Regionszustände, gemeinsame Quests.
Siedlungen, Bevölkerung, Gebäude, Bauprojekte und Wirtschaft kommen mit Phase 7 hinzu (neue Version).

## Versionierung & Migration

- `CharacterRecordVersion` / `WorldRecordVersion` (aktuell 1). Neuere Versionen als die unterstützte werden mit
  `UnsupportedVersion` abgelehnt, nie überschrieben.
- Beim Hinzufügen von Feldern: Version erhöhen, im `Read*Payload` nach `Version` verzweigen und Standardwerte
  setzen. Für jede Version wird ein Round-Trip-Test ergänzt.
- Questfortschritt wird nach dem Laden gegen aktuelle Definitionen abgeglichen (`FQuestLog::Reconcile`):
  hinzugefügte oder entfernte Ziele durch Content-Patches brechen keine Saves.
- Unbekannte Items (entfernter Content) werden beim Umräumen nie gelöscht.

## Wann wird gespeichert?

- Autosave alle 120 s (`AutosaveIntervalSeconds`): alle Charaktere + Welt.
- Beim Verlassen eines Spielers (`Logout`) und beim Beenden der Sitzung (`EndPlay`).
- Manuell: `DBSaveAll`.
- Geplant (Phase 19): zusätzliche Snapshots bei wichtigen Ereignissen (Levelaufstieg, Bosssieg, Questabschluss).

## Slots

| Slot | Inhalt |
|---|---|
| `DB_Character_<SlotKey>` | ein Charakter (lokal: `Slot0`… / Server: `Srv_<hash>_<index>`) |
| `DB_CharacterIndex` | Liste lokaler Charakter-Slots (Charakterauswahl) |
| `DB_World_<WorldSlot>` | eine Welt (`World0`, URL-Option `?World=`) |
