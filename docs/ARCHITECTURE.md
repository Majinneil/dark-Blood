# Architektur

## Leitprinzipien

1. **Serverautoritativ.** Schaden, Geld, Loot, Questfortschritt, Skillpunkte, Bossstatus und Charakterwerte
   werden ausschließlich auf dem Server verändert. Clients senden *Anfragen* (RPCs), der Server prüft sie.
2. **Regeln getrennt von der Engine.** Alle spielentscheidenden Regeln liegen im Modul `DarkBloodRules`:
   reines C++20 ohne Unreal-Header, ohne Exceptions und ohne RTTI. Es wird von UBT als UE-Modul **und** per
   CMake als Bibliothek für Unit-Tests gebaut. Damit sind die Regeln deterministisch testbar, auf einem Dedicated
   Server identisch und später auch für die Offline-Simulation (Siedlungen) nutzbar.
3. **Gameplay getrennt von Visuals.** Gameplay-Daten (HP, KI, Schaden, Loot, Queststatus) referenzieren
   Visuals nur über weiche Referenzen oder IDs. Platzhalter lassen sich ohne Codeänderung ersetzen.
4. **Datengetrieben.** Klassen, Items, Quests und Regionen sind Primary Data Assets, die der Asset Manager
   findet. Fehlen Assets, registriert der Code klar markierte Entwicklungsdaten (`[DEV]`).
5. **Persistenz auf dem PlayerState.** Inventar, Progression und Quests hängen am PlayerState, nicht am Pawn:
   Tod und Respawn verlieren nichts.

## Module

| Modul | Typ | Inhalt |
|---|---|---|
| `DarkBloodRules` | Runtime, ohne PCH | Namen, Progression, Stats, Schaden, Inventar/Taschen, Equipment, Todesstrafe, Skilltree-Regeln, Quests, Weltstatus, Archiv/Serialisierung, Datensätze + Validierung |
| `DarkBlood` | Runtime (Primary Game Module) | GAS, Enhanced Input, Charakter, Controller, GameMode/State, Komponenten, Subsysteme, SaveGame, Cheats, Debug-HUD |

Geplante weitere Module (bei Bedarf): `DarkBloodEditor` (Editor-Tools, Validierung), `DarkBloodUI` (CommonUI),
`DarkBloodSimulation` (Siedlungs-/Weltsimulation, Mass-Integration).

## Laufzeitobjekte

```
UGameInstance
 ├─ UDBGameDataSubsystem   lädt DBClass/DBItem/DBQuest/DBRegion-Assets → FItemCatalog, FQuestDatabase
 └─ UDBSaveSubsystem       Charakter-/Welt-Slots (SaveGame mit versioniertem Regelkern-Datensatz)

UWorld
 ├─ UDBQuestSubsystem      (Server) Ereignis-Routing → Questlogs, Belohnungen
 ├─ ADBGameMode            (Server) Charakterannahme, Validierung, Autosave, Tod/Respawn, Spawnpunkt
 ├─ ADBGameState
 │   ├─ UDBWorldStateComponent   Uhr, Tag/Nacht, Regionen, Vasallen, Story-Flags
 │   └─ UDBQuestComponent        gemeinsame (Story-)Quests
 ├─ ADBPlayerController    Input-Kontext, Charakter-Upload, Dev-Kommandos → Server
 ├─ ADBPlayerState         IAbilitySystemInterface
 │   ├─ UDBAbilitySystemComponent + UDBAttributeSet
 │   ├─ UDBProgressionComponent  Level, XP, Skillpunkte, Skill-Ränge, Gear-Score
 │   ├─ UDBInventoryComponent    Inventar, Taschen, Equipment, Währung, Nachlieferungen
 │   └─ UDBQuestComponent        persönliche Quests
 ├─ ADBPlayerController    + UDBDialogueComponent (Gespräche, serverseitig)
 ├─ ADBPlayerCharacter     Kamera, Bewegung, Input → Tags, Lock-On, Interaktion, Platzhalterkörper
 ├─ ADBEnemyCharacter      eigener ASC; ADBLesserDemon (+ UDBMeleeAIComponent), ADBTrainingDummy
 ├─ ADBNpcCharacter        Dialog-NPCs (IDBInteractable)
 ├─ ADBEncounterSpawner    Gegnergruppen nach Quest/Story-Flag
 ├─ ADBGameHUD             Slate-UI: HUD, Dialog, Charaktererstellung (+ Debug-Overlay)
 ├─ ADBRegionVolume        Regionserkennung (immer geladen)
 └─ ADBDebugHUD            Entwicklungs-Overlay
```

## Datenfluss einer Client-Aktion (Beispiel: Tasche ausrüsten)

1. UI/Client ruft `UDBInventoryComponent::RequestEquipBag(Slot)` → Server-RPC.
2. Server führt `FInventory::EquipBag` des Regelkerns aus. Der Taschenwechsel ist transaktional: Er läuft auf
   einer Kopie und wird nur bei Erfolg übernommen.
3. Bei Erfolg: `SyncReplicatedView()` aktualisiert die Fast-Array-Sicht → Delta-Replikation zum Besitzer.
4. Bei Fehler: `ClientRequestFailed(Operation, Grund)` → UI zeigt z. B. „InsufficientSpace“.

## Coding-Konventionen

- Unreal Coding Standard: Präfixe (`A`, `U`, `F`, `E`, `I`), PascalCase, Tabs, Allman-Klammern (`.clang-format`).
- Klassenpräfix `DB` (z. B. `ADBPlayerState`). Regelkern: Namespace `DarkBlood::Rules`, gleiche Präfixe.
- Quelltexte ASCII; UTF-8 nur in Texten/Daten. Kommentare Englisch, Dokumentation Deutsch.
- Kein Gameplay-Code in Level-Blueprints. Blueprints für Content (Fähigkeiten-Montagen, Encounter, Quests,
  Leveldesign), C++ für Systeme.
- Replizierte Zustände: Server hält die Wahrheit im Regelkern; UE-Properties sind nur die Sicht.
- `UFUNCTION`s geben Container by value zurück (keine Referenzen an Blueprints).
- Jede serverseitige Mutation prüft `HasAuthority()`.

## Verzeichnisstruktur Content (Soll)

```
/Game/DarkBlood/
  Data/{Classes,Items,Quests,Regions,Enemies,Bosses}
  Input/                  IA_*, IMC_*, DA_InputConfig
  Characters/{Player,NPC,Demons,Vassals,DemonKing}
  Abilities/{Common,Warrior,Shadowrunner,Mage,Monk,Bosses}
  Maps/L_Realm            eine World-Partition-Hauptwelt
  Maps/Dungeons/          nur für Dungeons mit eigenem Streaming, falls nötig
  UI/  VFX/  Audio/  Cinematics/
  Dev/                    eindeutig markierte Entwicklungsassets
```

## Entwicklerkommandos

Alle Kommandos laufen auf dem Server (Clients leiten automatisch weiter; nur wenn der GameMode Cheats erlaubt,
z. B. im PIE; in Shipping-Builds deaktiviert).

| Kommando | Wirkung |
|---|---|
| `DBGiveXp <n>` | XP vergeben (Levelaufstieg, Attributneuberechnung) |
| `DBSetLevel <n>` | Level direkt setzen |
| `DBGiveSkillPoints <n>` | Fähigkeitspunkte |
| `DBGiveItem <ItemId> <n>` | Items hinzufügen (Rest wird verworfen, Meldung im Log) |
| `DBGiveCurrency <n>` | Mon hinzufügen |
| `DBEquipBag <Section> <Index>` | Tasche aus Slot ausrüsten (normaler RPC-Pfad) |
| `DBEquipItem <Section> <Index> <Slot>` | Item ausrüsten (`MainHand`, `Head`, ...) |
| `DBStartQuest <QuestId>` | Quest starten |
| `DBQuestEvent <Kind> <Target> <n>` | Questereignis melden (`Kill`, `Collect`, `Reach`, `Talk`, `Interact`, `Custom`) |
| `DBSetStoryFlag <Flag>` | Story-Flag setzen |
| `DBDefeatBoss <BossId> <Rank> <RegionId>` | Bosssieg (`WorldBoss`, `MidBoss`, `Vassal`, `DemonKing`) |
| `DBSetTime <Stunde>` | Tageszeit (Zeit läuft nie rückwärts) |
| `DBDamageSelf <n>` | Schaden über das Schadens-Meta-Attribut (testet den Todesablauf) |
| `DBHeal` | Vitalwerte auffüllen |
| `DBSaveAll` | Alle Charaktere und die Welt speichern |
| `DBDumpCharacter` / `DBDumpWorld` | Zustand ins Log |
| `DBToggleDebugHUD` | Debug-Overlay ein/aus (lokal) |
| `DBSpawnDummy` / `DBSpawnEnemy [cm]` | Trainingspuppe / niederen Dämon vor dem Spieler erzeugen |
| `DBInput <Aktion> [s]` | Eingabe drücken/halten (`LightAttack`, `HeavyAttack`, `Dodge`, `Block`, `Sprint`, `Interact`, `Ability.1` …) |
| `DBJump`, `DBLockOn` | Springen / Zielerfassung (lokal) |
| `DBAfter <s> <Kommando>` | Kommando zeitversetzt ausführen (Testskripte) |
| `DBDummyAttack`, `DBDummyAutoAttack <s>` | Trainingspuppen schlagen zu |
| `DBDumpCombat` | Leben/Ausdauer/Poise/Tags aller Kämpfer |
| `DBSetupSlice` | Story-Slice um den Spieler aufbauen |
| `DBGoto <Id>` | Zum NPC/Gegner teleportieren (`NPC_King`, `TrainingDummy`, `LesserDemon` …) |
| `DBDialogueChoose <n>` | Dialogoption wählen (lokal) |
| `DBCreateCharacter <Klasse> <Name>` | Charaktererstellung abschließen (lokal) |
| `DBUnlockSkill <Knoten>` | Skilltree-Knoten lernen (normale Regeln) |
| `DBUse <Item>`, `DBEquipById <Item>` | Verbrauchsgut benutzen / Ausrüstung anlegen |
| `DBCraft <Rezept>`, `DBRepair` | An der nächsten Station herstellen / reparieren |
| `DBGrantLoot <Tabelle>` | Beutetabelle für den Spieler würfeln |
| `DBVisualSlice 0\|1` | Visual Slice entfernen/aufbauen (alle Rechner) |
| `DBTimeOfDay Day\|Dusk\|Night\|DemonNight` | Lichtstimmung des Visual Slice |
| `DBVisuals 0\|1` | Charakter-Profile an / Greybox-Körper (lokal) |
| `DBView X Y Yaw [Pitch]` | Spieler relativ zum Slice-Ursprung platzieren |
| `DBOrbit Yaw [Pitch] [Abstand]` | Kamera um die eigene Figur drehen (180 = von vorn), für Sichtprüfungen |
| `DBGraphics Qualität [Hochskalierung] [Raytracing]` | Grafik für Tests setzen (0–4, 0 = nativ … 4 = 33 %, 0/1); Spieler nutzen das Menü `[F10]` |
| `DBPerfSnapshot` / `DBVisualAudit` | Frame-/GPU-Zeit, Draw Calls, Slice-Statistik / fehlende Visual-Referenzen |
| `DBTravel <Region\|Siedlung> [X m] [Y m]` | In ein Gebiet / eine Siedlung der offenen Welt springen, optional versetzt |
| `DBWalk <Gierwinkel> <s>` | Eigene Figur über normale Bewegungseingabe laufen/schwimmen lassen, im Sattel das Pferd lenken (lokal; 0 = Osten) |
| `DBDumpSettlements` / `DBDumpVillagers` | Zustand aller Siedlungen / Positionen der Dorfbewohner |
| `DBSkipHours <h>` | Spielzeit vergehen lassen (Siedlungen holen auf) |
| `DBSettlementAttack <Siedlung>` | Dämonenangriff als Kampf in der Siedlung (Spieler muss dort sein) |
| `DBRide`, `DBCarriage <Siedlung>` | Eigenes Pferd rufen + auf-/absteigen / Kutsche von der nächsten Station |
| `DBSurvival <Sättigung> <Wärme>` | Survival-Werte setzen |
| `DBDungeonEnter <Name>` / `DBDungeonRoom <Index\|Art>` / `DBDungeonDump` | Dungeon betreten / zu einem Raum springen / Zustand aller Dungeons |
| `DBKillNearby <m>` | Gegner im Umkreis besiegen (Tests) |
| `DBBossList` / `DBBossArena <Name\|Nr.\|King>` / `DBBossSpawn <Name> [Abstand]` | Bosse und Arenen auflisten / in eine Arena springen / Boss vor dem Spieler erzeugen |
| `DBBossDump` / `DBBossHurt <Anteil>` / `DBBossDefeat <Name\|Outer>` | Boss-Zustand / Lebensanteil setzen (Phasen) / als besiegt eintragen (`Outer` = 14 Vasallen draußen) |
| `DBRegionDump` / `DBCamp <Nr.\|Name>` / `DBRegionPack` | Gebietszustand, Rudel und Lager / zum Dämonenlager springen / Rudel sofort erzeugen |
| `DBBossSignature` / `DBEndGate` / `DBBastion` | Signatur-Attacke des nächsten Bosses / vor das Tor des Endes / zur Letzten Bastion |
| `DBParadiseGate` / `DBParadise [Shrine]` / `DBFinale` | vor die Pforte am Thron / auf die Paradies-Insel (vor den Schrein) / Finale anzeigen |

Kommandozeile: `-DBPersistence=Local|Server`, `-DBCharacterSlot=<Slot>`, `-DBCharacterName="Jin Akagi"` (überspringt die
Charaktererstellung), `-DBCharacterClass=<Klasse>`, `-DBSkipCreator`, `-DBDevSlice`, `-DBCheats` (Dev-Kommandos auf Listen-/Dedicated-Servern),
`-DBAutoExec="Cmd|Cmd"` (Skript nach dem Verbinden, auch auf Clients), `-DBNoVisualSlice`, `-DBTimeOfDay=<Preset>`,
`-DBGreybox` (Charaktere ohne Profile);
URL-Optionen: `?World=<Slot>`, `?Character=<Index>` (Dedicated Server).
