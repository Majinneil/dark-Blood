# DAS ENDE (Phase 13)

**Status:** ✅ Gameplay (headless, gerendert getestet) · 🟡 schwebende Inseln und Klippen der Kartenvorlage
(Geländeneubau, Phase 17), eigene Architektur der Dämonenfestung, Musik.

DAS ENDE ist das Land des Dämonenkönigs im Südosten. Es ist kein abgesperrtes Gebiet: Wer stark genug ist, kann
hinein (Dämonen der Stufe 62–70, „Blutmond-Dämonen“, auch nachts mehr). Versiegelt sind die Arenen von Tsukigami und
Shirogane, bis die 14 Vasallen draußen gefallen sind, und der Thron, bis auch diese beiden fallen (Phase 10).

## Tor des Endes

`ADBEndGate`: drei verderbte Torii hintereinander an der Seite von DAS ENDE, die zum Reich zeigt, gesäumt von
verderbten Steinlaternen. Im mittleren Torii hängt ein **Blutsiegel** (leuchtende, halbdurchsichtige Fläche), solange
die 14 äußeren Vasallen nicht besiegt sind. Das Schild zeigt den Stand („versiegelt (9/14 Vasallen)“). Das Siegel ist
eine Story-Markierung, kein Hindernis.

Wenn das Siegel bricht, hören das alle Spieler, und das Tor startet die Hauptquests von DAS ENDE nacheinander:

| Quest | Ziele | Belohnung |
|---|---|---|
| `MQ10_TheEndSeal` „Das Siegel des Endes“ | in der Letzten Bastion rasten; durch das Tor in DAS ENDE gehen | 4000 EP, 1 Fähigkeitspunkt |
| `MQ11_ThroneGuardians` „Die Wächter des Throns“ | Tsukigami und Shirogane besiegen | 8000 EP, 2 Fähigkeitspunkte |
| `MQ12_DemonKing` „Der Dämonenkönig“ | den Dämonenkönig stürzen (Phase 14) | 20000 EP, 3 Fähigkeitspunkte |

Bosse, die vor dem Start einer Quest fielen, werden beim Start angerechnet (gilt für alle Quests).

## Pfad der Schande

Verderbte Torii alle 130 m vom Tor bis zur Thron-Arena, außerhalb der Arenen.

## Letzte Bastion

`ADBBastionShrine`: das letzte Lager der Menschen vor dem Tor, außerhalb von DAS ENDE. Zelte, Palisade gegen DAS ENDE,
Banner der Hauptstadt, Feuerschein (ein schattenloses Licht). Rasten heilt wie ein Schrein und macht die Bastion zum
**Ruhepunkt**: Wer stirbt, kehrt dorthin zurück (`APlayerStart` mit `Bastion_TheEnd`).

Dabei behoben: Wiederbelebungen landeten immer am ersten Startpunkt, weil der zwischengespeicherte `StartSpot` den
Ruhepunkt überging. Er wird vor jeder Wiederbelebung verworfen.

## Testbefehle

| Befehl | Wirkung |
|---|---|
| `DBEndGate` | 25 m vor das Tor des Endes springen (Blick hinein) |
| `DBBastion` | zur Letzten Bastion springen |
| `DBBossDefeat Outer` | die 14 äußeren Vasallen als besiegt eintragen (das Siegel bricht) |
