# Endgame (Phase 16)

Nach dem Fall des Dämonenkönigs geht DARK BLOOD weiter: drei Systeme, die alle auf dem Regelkern
`DarkBloodRules/Endgame.h` beruhen (reine Regeln, getestet in `Tests/RulesTests/EndgameTests.cpp`) und im Weltstand
gespeichert werden (Weltdatensatz Version 5: Zyklus, erinnerte Bosse, Echo-Ränge, tiefste Abgrund-Ebene).

| System | Wo | Freigeschaltet |
|---|---|---|
| **New Game+** (Zyklen) | Pforte des Blutmonds auf der Paradies-Insel | nach dem Sieg über den Dämonenkönig (Welt gereinigt) |
| **Halle der Echos** (Revanchen) | Tor vor der Hauptstadt → Halle über dem Westmeer | sobald der König einmal gefallen ist |
| **Der Abgrund** (endloser Dungeon) | Riss vor der Hauptstadt | sobald der König einmal gefallen ist |

## 1. New Game+

Die **Pforte des Blutmonds** (`ADBParadiseGate`, Art `NewCycle`) steht auf der Paradies-Insel hinter dem Teich und
öffnet sich mit der Pforte am Thron. Erste Benutzung: Warnung mit den Werten des nächsten Zyklus; zweite Benutzung
innerhalb von 20 s bestätigt (`ADBGameMode::BeginNewCycle`):

- Alle Vasallengebiete und DAS ENDE fallen zurück an die Dämonen (besetzt, Einfluss 1), die Vasallen kehren auf ihre
  Throne zurück (Arenen öffnen wieder), Dungeons sind wieder unbezwungen, Story-Flags und die gemeinsame Hauptquest
  beginnen neu (`InitialSharedQuests`).
- **Bleibt**: Charaktere (Stufe, Fähigkeiten, Ausrüstung, Inventar), Siedlungen, Uhrzeit, Abgrund-Tiefe, Echo-Ränge.
- Jeder besiegte Boss wird in `RememberedBosses` übernommen (für die Halle der Echos).
- Alle Spieler im Paradies kehren in die Hauptstadt zurück; die Welt wird sofort gespeichert.

Skalierung je Zyklus (`GetCycleScale`, gedeckelt bei NG+7):

| | pro Zyklus | NG+1 | NG+3 | NG+7 (Gipfel) |
|---|---|---|---|---|
| Leben der Dämonen | +60 % | ×1,6 | ×2,8 | ×5,2 |
| Schaden | +30 % | ×1,3 | ×1,9 | ×3,1 |
| Erfahrung | +50 % | ×1,5 | ×2,5 | ×4,5 |
| Beute-Seltenheit | +0,06 | +0,06 | +0,18 | +0,42 |
| Stufe | +10 | +10 | +30 | +70 |

Angewendet in `ADBEnemyCharacter::ApplyEndgameScale` beim Spielbeginn jedes Dämons (Server) – damit greift sie
überall gleich: Rudel, Lager, Dungeons, Vasallen, König. Übungspuppen und Nicht-Dämonen bleiben unverändert.
Die Wut eines Bosses rechnet mit dem skalierten Angriff. Das HUD zeigt den Zyklus über dem Questtracker.

**Beute** (`ApplyEndgameLoot`): jeder Eintrag einer Beutetabelle wiegt `× (1 + Bonus × Seltenheitsstufe)`, die Chance
auf „nichts“ sinkt um `Bonus`, Münzen steigen um `2 × Bonus`. Gilt für Dämonen, Bosse und Truhen
(`UDBInventoryComponent::GrantLootTable`, `ADBLootChest`).

## 2. Halle der Echos

Ein Ring aus Säulen über dem Westmeer (`ADBEchoHall`), 17 Gedenksteine (`ADBEchoStone`) – 16 Vasallen und der König.
Ein Stein erwacht, sobald sein Boss in dieser Welt gefallen ist (in diesem oder einem früheren Zyklus) und zeigt den
Echo-Rang. Berühren ruft das **Echo**: der Boss in geisterhaftem Blau (`ADBBossCharacter::bEcho`), mit allen Phasen und
Signatur-Attacken, skaliert nach `GetEchoScale(Zyklus, Rang)`:

- je Rang +35 % Leben, +20 % Schaden, +2 Stufen, +0,02 Beute-Seltenheit (Rang 10 ist das Maximum), multipliziert mit
  dem Zyklus.
- Ein Echo verändert die Welt nicht: kein Gebiet, keine Story, keine Fähigkeitspunkte – Erfahrung, Beute und der
  nächste Rang (`FWorldState::RecordEchoVictory`).
- Immer nur ein Echo zugleich; verlassen alle Spieler die Halle für 8 s, verblasst es.

## 3. Der Abgrund

Ein Dungeon ohne Ende (`FDBDungeonSite::bAbyss`). Jede Ebene wird aus Tiefe und Zyklus erzeugt
(`GetAbyssFloor`: fester Seed → gleiche Ebene, gleiches Layout auf allen Rechnern):

| Ebene | Räume | Dämonen je Raum | Leben | Schaden | Stufe | Beute |
|---|---|---|---|---|---|---|
| 1 | 4 | 2 | ×1,0 | ×1,0 | +0 | +0 |
| 5 (Wächter) | 5 | 3 | ×1,48 | ×1,28 | +4 | +0,04 |
| 10 (Wächter) | 7 | 4 | ×2,08 | ×1,63 | +9 | +0,09 |
| 30+ | 9 | 6 | ×4,5 … | ×3,0 … | +29 … | +0,3 (Deckel) |

(jeweils noch × Zyklus-Skalierung.) Jede fünfte Ebene endet mit dem Wächter, sonst wartet im letzten Raum ein
Elite-Rudel. Ist die Ebene frei: Erfahrung (90 × Tiefe × Zyklus-XP), der Hort, ein Ausgang und die **Treppe hinab** –
die ganze Gruppe steigt gemeinsam ab, die nächste Ebene steht schon fertig daneben (zwei Bauplätze im Wechsel), die
alte verschwindet. Der Abgrund beginnt nach dem letzten besiegten Wächter (Ebene 1, 6, 11 …); eine Gruppe, die schon
unten ist, nimmt Nachzügler auf ihrer Ebene auf. Leere Ebenen verschwinden nach 90 s.

Aussehen: kalter alter Fels, das Dunkle Blut glüht nur in der Naht zwischen Wand und Boden, magentafarbenes
Fackellicht, rot im Raum des Wächters.

## Testbefehle

| Befehl | Wirkung |
|---|---|
| `DBEndgame` | Zyklus, Skalierung, Abgrund-Tiefe, erinnerte Bosse und Echo-Ränge |
| `DBNewCycle [force]` | nächsten Zyklus beginnen (`force`: König vorher als besiegt werten) |
| `DBAbyss <Ebene>` / `DBDescend` | Abgrund-Ebene betreten / von einer freien Ebene absteigen |
| `DBEchoHall` / `DBEcho <Boss>` | Halle betreten / Echo eines Bosses rufen |

## Getestet (2026-10-07)

- Regelkern 72/72 (Zyklus-Skalierung und Deckel, neuer Zyklus setzt die Welt zurück und behält die Erinnerung,
  Echo-Ränge, Abgrund-Ebenen deterministisch, Speichern/Laden, Beute-Bonus).
- Headless: Abgrund Ebene 1 → frei → Treppe → Ebene 2 (alte Ebene entfernt); Halle versiegelt vor dem König;
  NG+1 beginnt (2 erinnerte Bosse, Hauptquest neu, Arena Akakage wieder offen); Akakage in NG+1: Stufe 34, 3840 Leben
  (×1,6); Echo Rang 1 (×2,16 Leben) → besiegt → Rang 2 (×2,72); Ebene 5 mit Wächter (Stufe 24, ×2,37); nach Neustart
  sind Zyklus, Echo-Rang und Abgrund-Tiefe geladen.
- Koop (Listen-Server + Client): Client in der Halle sieht das Echo (repliziert), Client und Host auf derselben
  Abgrund-Ebene (gleicher Seed), beide steigen gemeinsam ab, der Client baut beide Ebenen lokal.
- Gerendert (1600 × 900): Halle der Echos mit Echo, Abgrund-Ebene, Steinlaternen-Schrein, Pforte des Blutmonds; 93 FPS
  im Paradies.

## Offen

- Eigene Modelle für Gedenksteine und Abgrund-Architektur, Musik je Ebene, Ahnengeister im Paradies (Phase 17/18).
- Ranglisten / wöchentliche Abgrund-Seeds (braucht Online-Dienst).
