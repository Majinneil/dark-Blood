# Paradies und Finale (Phase 15)

**Status:** ✅ Gameplay (Regelkern, headless, Koop, gerendert getestet) · 🟡 Zwischensequenz, Musik, Ahnengeister
mit Dialogen, Neues Spiel+ (Phase 16).

## Die Welt nach dem Dämonenkönig

Fällt der Dämonenkönig, wird die Welt gereinigt (`FWorldState::PurifyWorld`): Alle 14 Vasallengebiete und DAS ENDE
sind befreit, der Dämoneneinfluss liegt bei 0, `Story.WorldPurified` ist gesetzt. Es erscheinen keine Dämonenrudel mehr,
auch nicht in DAS ENDE und auch nachts nicht. Die Siedlungen erholen sich (Phase 7).

## Die Pforte aus Licht

`ADBParadiseGate` (Art `ToParadise`) steht unsichtbar im Herzen der Thron-Arena und öffnet sich, sobald
`Story.DemonKingDefeated` gesetzt ist (alle Spieler hören es): ein heller Torii mit goldenem Lichtvorhang. Wer hindurch
schreitet, steht im Paradies.

## Das Paradies

`ADBParadiseIsland`: eine schwebende Insel 1,5 km über dem Thron. Sie ist von DAS ENDE aus am Himmel zu sehen und greift
die schwebenden Inseln der Weltkarte auf. Ausstattung:
- Wiese auf einem Felskegel, drei kleine schwebende Felsen ringsum,
- Steinpfad mit Laternen vom Ankunftstor zum Schrein, ein stiller Teich,
- Kirschbäume und rote Ahorne am Rand, eine Pagode und ein Schreinhaus (Sketchfab, CC BY),
- eigene Stimmung: klare, goldene Luft statt des Blutnebels darunter (`UDBRealmMoodComponent`),
- Gebiet „Paradies“ (`Paradise`, Epilog): keine Dämonen.

Der Blick von der Insel reicht über den ganzen Kontinent.

**Schrein des Friedens** (`ADBPeaceShrine`): Verweilen heilt und zeigt allen Spielern auf der Insel das **Finale**
(`ADBPlayerController::ClientShowFinale` → `ADBGameHUD::ShowFinale`, 30 s): „DARK BLOOD – Das Dunkle Blut ist
verstummt“, Abschlusstext und Credits. Danach geht die Welt weiter: Das Rückkehr-Tor (`ADBParadiseGate` Art `Home`)
bringt zurück in die Hauptstadt.

## Quest

`MQ13_Paradise` „Das Paradies“ (nach `MQ12_DemonKing`, vom Tor des Endes gestartet): durch die Pforte aus Licht
schreiten, am Schrein des Friedens verweilen. Belohnung: 5000 EP, 1 Fähigkeitspunkt, `Story.Finale`.

## Testbefehle

| Befehl | Wirkung |
|---|---|
| `DBParadiseGate` | 3 m vor die Pforte am Thron |
| `DBParadise [Shrine]` | auf die Insel (vor das Rückkehr-Tor) bzw. vor den Schrein des Friedens |
| `DBFinale` | Finale-Bildschirm anzeigen |
| `DBBossDefeat King` | den Dämonenkönig als besiegt eintragen (Welt wird gereinigt, Pforte öffnet sich) |
