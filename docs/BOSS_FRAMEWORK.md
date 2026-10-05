# Boss-Framework

**Status:** Phase 10 – Gameplay ✅ (Regelkern, headless, Koop, gerendert getestet) · 🟡 eigene Modelle, Animationen,
Musik, Intros. Entwürfe der Bosse: `References/DARK_BLOOD_16_VASALLEN_UND_DAEMONENKOENIG.pdf`.

## 16 Vasallen und der Dämonenkönig

13 Vasallen beherrschen je ein Gebiet (Region01–Region13), **Reikon** die Vasallenfestung (Region14). Wer ein Gebiet
befreien will, besiegt dessen Vasallen. Nach den 14 Vasallen draußen öffnet sich **DAS ENDE**; dort bewachen
**Tsukigami** (Blutmond) und **Shirogane** (Rechte Hand des Königs) den Weg. Erst wenn beide gefallen sind, ist der
**Dämonenkönig** erreichbar.

| Nr. | Vasall | Titel | Gebiet | Element | Mechaniken (Phase 1 → ab 50 %) |
|---|---|---|---|---|---|
| 1 | Akakage | Vasall des Blutes | Kirschblütental | Blut | Sturmangriff, Gefahrenzone → + Stampfer |
| 2 | Yukimaru | Vasall des Frosts | Eisöde | Frost | Salve, Gefahrenzone → + Stampfer |
| 3 | Kurobane | Vasall des Schattens | Bambuswälder | Schatten | Sturmangriff, Beschwörung → + Salve |
| 4 | Raikyo | Vasall des Donners | Nebelberge | Donner | Salve, Stampfer → + Gefahrenzone |
| 5 | Enkazan | Vasall von Feuer und Asche | Feuergebirge | Feuer | Stampfer, Gefahrenzone → + Sturmangriff |
| 6 | Shikotsu | Vasall von Knochen und Tod | Wüstenlande | Geist | Beschwörung, Stampfer → + Gefahrenzone |
| 7 | Dokuga | Vasall von Gift und Seuche | Flusslande | Gift | Gefahrenzone, Salve → + Beschwörung |
| 8 | Mugenrei | Vasall der Illusion | Wald der Geister | Geist | Salve, Beschwörung → + Sturmangriff |
| 9 | Juragan | Vasall der Bestien | Reisfelder | physisch | Sturmangriff, Beschwörung → + Stampfer |
| 10 | Tetsukhan | Vasall des Eisens | Großstadt | physisch | Stampfer, Sturmangriff → + Beschwörung |
| 11 | Kujiraa | Vasall der Tiefen | Küstenland | Frost | Gefahrenzone, Salve → + Stampfer |
| 12 | Hayate | Vasall des Windes | Himmelstempel | physisch | Sturmangriff, Salve → + Gefahrenzone |
| 13 | Kokuya | Vasall der Finsternis | Dämonenöde | Schatten | Beschwörung, Gefahrenzone → + Sturmangriff |
| 14 | Reikon | Vasall der Leere | Vasallenfestung | Blut | Salve, Gefahrenzone → + Beschwörung |
| 15 | Tsukigami | Vasall des Blutmondes | DAS ENDE | Blut | Sturmangriff, Salve → + Gefahrenzone, Beschwörung |
| 16 | Shirogane | Rechte Hand des Dämonenkönigs | DAS ENDE | physisch | Stampfer, Sturmangriff, Beschwörung → + Salve |

**Dämonenkönig** (12 000 Leben, Thron im Herzen von DAS ENDE): drei Formen bei 100 / 66 / 33 % Leben –
*Dämonischer Kaiser* → *Dark-Blood-Korruption* → *Vollständige Dämonenform*. Er wächst mit jeder Form
(1,8 → 2,1 → 2,6) und bekommt neue Mechaniken. Sein Tod setzt `Story.DemonKingDefeated`.

Dazu der **Dungeon-Wächter** (`B_DungeonGuardian`, Rang WorldBoss) im Bossraum jedes Dungeons. Seine Werte wachsen mit
der Dungeon-Stufe (×0,5 + 0,25 je Stufe).

## Signatur-Attacken und Kampfsprüche (Phase 12)

Jeder Vasall hat zusätzlich zu den gemeinsamen Mechaniken eine eigene Attacke (`EDBBossSignature`). Er setzt sie alle
`GetSignatureCooldown` Sekunden ein (18 s, je Phase 3 s weniger, mindestens 9 s, im Koop kürzer) und sofort nach dem
Phasenwechsel. Zu Kampfbeginn und beim Phasenwechsel spricht er (Meldung an alle im Kampf).

| Vasall | Signatur | Wirkung |
|---|---|---|
| Akakage | Blutpfad | Blutlachen entlang des Wegs, dann ein Sturmangriff, der ihn bei jedem Treffer um 4 % heilt |
| Yukimaru | Eisringe | drei Ringe aus Frostzonen, die sich nacheinander nach außen ausbreiten |
| Kurobane | Schattenschritt | springt hinter den entferntesten Spieler und schlägt zu |
| Raikyo | Blitzregen | je Spieler drei Blitzeinschläge, der erste genau auf ihm |
| Enkazan | Flammenwall | ein Feuerring um ihn (6 s), der zum Nahkampf zwingt |
| Shikotsu | Knochenarmee | vier Knochenkrieger |
| Dokuga | Seuchenwolke | große Giftzone, die 8 s brennt |
| Mugenrei | Trugbilder | zwei Trugbilder in seiner Größe kämpfen mit |
| Juragan | Rudelruf | drei schnelle Bestien, er selbst +30 % Angriff für 10 s |
| Tetsukhan | Eisenhaut | 6 s dreifache Rüstung, danach ein Beben |
| Kujiraa | Flutwelle | eine dreispurige Welle rollt auf das Ziel zu |
| Hayate | Sturmklingen | drei Sturmangriffe hintereinander auf zufällige Spieler |
| Kokuya | Klingen der Nacht | Geschosse in alle Richtungen (mehr je Phase) |
| Reikon | Leerensog | zieht alle Spieler zu sich, dann bricht die Leere aus |
| Tsukigami | Blutmond | heilt 6 % und feuert einen Geschossring |
| Shirogane | Banner des Königs | zwei Silberwachen; solange sie stehen, hat er stark erhöhte Rüstung |

Beschworene Dämonen haben jetzt die Stärke des Bosses (wenige Stufen darunter) statt der eines Grunddämons, und
Dämonen eines Gebiets zählen für dessen Befreiungsquest. Testbefehl: `DBBossSignature` (nächster Boss).

## Umsetzung

| Baustein | Datei | Beschreibung |
|---|---|---|
| Regeln | `DarkBloodRules/Boss.h` | Koop-Skalierung `GetBossScaling`, Phasen `EvaluateBossPhase` (nie zurück), Raserei `GetEnrageMultiplier` |
| Weltstatus | `DarkBloodRules/WorldState.h` | 16 Vasallen (`NumVassals`), je Vasall gezählt (`MarkVassalDefeated`), DAS ENDE ab 14 befreiten Gebieten, `IsDemonKingReachable`; Weltstand v4 |
| `UDBBossDefinition` | `Boss/DBBossDefinition.h` | Id, Name, Titel, Rang, Gebiet, Element, Farbe, Werte, Belohnung, Phasen (Schwelle, Mechaniken, Größe, Name), Raserei, Arenaradius, Porträt und Visual-Profil als weiche Referenz. DEVELOPMENT: im Code angelegt (`DBBosses::GetAll`) |
| `ADBBossCharacter` | `Boss/DBBoss.h` | Nahkampf-KI plus Spezialangriffe, Phasenwechsel mit 2 s Unverwundbarkeit, Raserei, Koop-Skalierung, Belohnung für alle Kämpfer |
| `ADBBossTelegraph` | `Boss/DBBoss.h` | Warnring, der wächst und dann trifft; Zonen brennen weiter (1 Treffer/s) |
| `ADBBossArena` | `Boss/DBBoss.h` | Steinboden, 12 Steinlaternen (Sketchfab), zwei Tempel-Wächterstatuen (Fab, MTSU-Fotogrammetrie), Torii als Eingang (Fab), leuchtende halbdurchsichtige Blutbarriere in Bossfarbe (`M_DB_BloodBarrier`, `Tools/UE58/db_create_barrier_material.py`) während des Kampfs, Versiegelung (DAS ENDE, Thron), Zurücksetzen, wenn 5 s niemand im Ring ist |
| Boss-Leiste | `UI/SDBGameHudWidget` | nächster lebender Boss im Umkreis von 60 m: Porträt (aus den Konzeptblättern, `Tools/UE58/db_import_boss_portraits.py`), Name, Titel, Phase, Leben |
| Kartenmarker | `UI/SDBWorldMapWidget` | Quadrat in Bossfarbe je Arena, grau nach dem Sieg |

### Spezialangriffe

| Mechanik | Ablauf |
|---|---|
| Stampfer | Warnring um den Boss (Radius 4,2 m × Größe), nach 1,3 s Treffer mit Niederschlag |
| Sturmangriff | Sprint auf das Ziel, trifft jeden auf dem Weg (0,6 s) mit Niederschlag |
| Salve | 3 + Phase elementare Geschosse im Fächer |
| Gefahrenzone | Zone unter dem Ziel (ab 3 Spielern unter **jedem**), nach 1 s für 4 s ein Treffer pro Sekunde |
| Beschwörung | 2 + zusätzliche Dämonen je weiterem Spieler, höchstens 6 gleichzeitig |

Die Abklingzeit sinkt mit jeder Phase (8 s − 1,5 s je Phase, mindestens 2,5 s) und mit der Spielerzahl.

### Koop-Skalierung

Nicht nur mehr Leben. Je Spielerzahl 1 / 2 / 3 / 4:

- Leben ×1 / ×1,45 / ×1,85 / ×2,2 (moderater letzter Regler)
- ein zusätzlicher Dämon pro weiterem Spieler bei Beschwörungen
- ab 2 Spielern **geteilte Aufmerksamkeit**: nach jedem Spezialangriff wendet sich der Boss einem anderen Spieler zu
- ab 3 Spielern **Flächendruck**: Gefahrenzonen unter allen Spielern
- Abklingzeiten ×1 / ×0,9 / ×0,85 / ×0,8

Raserei: Nach `EnrageAfterSeconds` (Vasallen 300 s, König 420 s) steigt der Schaden alle 30 s um 25 %, höchstens ×2.

### Belohnung

Jeder Spieler im Kampf (45 m um den Boss) erhält die Erfahrung, Fähigkeitspunkte (Vasall 1, König 3) und eigene
Beute (`LT_Vassal`, `LT_DemonKing`, `LT_DungeonGuardian`; keine Beute-Konkurrenz). Ein Vasall befreit sein Gebiet über
`UDBWorldStateComponent::NotifyBossDefeated`. Der Sieg wird gespeichert, die Arena bleibt danach grau.

### Arenen

`DBBosses::GetArenaLocation` sucht nahe dem Wunschort die flachste trockene Stelle: innerhalb der Welt, fern von
Siedlungen und Dungeon-Toren, die drei Arenen von DAS ENDE mindestens 350 m auseinander. Das Ergebnis ist
deterministisch, Server, Clients und Karte sind sich also einig, und es wird je Boss zwischengespeichert. Die Arena
räumt eingebackene Bäume und Felsen in ihrem Ring weg (auf jedem Rechner, Vegetation wird nicht repliziert).

## Trennung Gameplay/Visual

Die Boss-Logik kennt kein Mesh. Bis die Heldenmodelle aus den Entwürfen existieren, ist jeder Boss ein eingefärbter
Dämonenkörper mit Aura in seiner Farbe; die Größe kommt aus der Phase. `VisualProfileId` und `Portrait` sind weiche
Referenzen und werden ausgetauscht, ohne Gameplay zu ändern.

## Testbefehle

| Befehl | Wirkung |
|---|---|
| `DBBossList` | alle Bosse mit Arena und Zustand, Stand der Vasallen / DAS ENDE / Thron |
| `DBBossArena <Name\|Nr.\|King>` | in die Arena springen (startet den Kampf, wenn offen) |
| `DBBossSpawn <Name> [Abstand]` | Boss vor dem Spieler erzeugen |
| `DBBossDump` | Bosse in der Nähe: Phase, Leben, Angriffskraft, Kampfzeit, Spezialangriffe, Dämonen; Arenen; Spielerpositionen |
| `DBBossHurt <Anteil>` | nächsten Boss auf einen Lebensanteil setzen (Phasen testen) |
| `DBBossDefeat <Name\|Outer>` | Boss als besiegt eintragen; `Outer` = die 14 Vasallen draußen |

## Offen

Eigene Modelle, Animationen und Telegraph-VFX je Boss; Musik; Intros und Finisher (Level Sequences, Motion Warping);
StateTree-KI pro Boss mit eigenen GAS-Abilities statt der gemeinsamen Mechanik-Bausteine; Arena-Transformation des
Königs; Checkpoints.
