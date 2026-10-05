# Regionsinhalte (Phase 11)

**Status:** ✅ Gameplay (Regelkern, headless, Koop, gerendert getestet) · 🟡 eigene Gegnermodelle je Gebiet,
Gebiets-Landmarken, Musik.

Die 14 Vasallengebiete und DAS ENDE sind voller Dämonen. Gefahr entsteht über Stärke, nicht über Sperren: Jedes Gebiet
hat sein Stufenband (`UDBRegionDefinition::RecommendedPowerMin/Max`, Gebiet n: 4n−1 bis 4n+5, DAS ENDE 62–70).

## Dämonenrudel

`UDBRegionLifeComponent` (am `ADBRealmDirector`, nur Server) hält um jeden Spieler Rudel aus drei Dämonen des Gebiets
(„Blutdaemon“ im Kirschblütental, „Frostdaemon“ in der Eisöde …). Sie erscheinen 45–75 m entfernt auf freiem, trockenem
Boden: nicht in Siedlungen, nicht in Bossarenen, nicht im Lager.

| Gebietszustand | Rudel tagsüber | Rudel nachts |
|---|---|---|
| besetzt (Vasall herrscht) | 2 (Anführer: Elite, +3 Stufen, ×1,6) | 3 |
| umkämpft (Lager zerschlagen) | 1 | 2 |
| befreit (Vasall besiegt) | 0 | 1, solange der Dämoneneinfluss über 0,2 liegt |
| DAS ENDE | 2 | 3 |
| Hauptstadt, Paradies | 0 | 0 |

- **Stufe:** `GetRegionalDemonLevel` (im Band, deterministisch je Spawn). **Werte:** `GetRegionalStatMultiplier`
  (+12 % je Stufe über dem Grunddämon), angewendet mit `ADBEnemyCharacter::ConfigureSpawn`. Name und Stufe werden
  repliziert (Namensschild).
- **Koop:** Rudel im Umkreis von 100 m zählen für alle Spieler dort, so dass vier Spieler nebeneinander nicht viermal so
  viele Dämonen bekommen. Pro Spieler höchstens alle 40 s ein neues Rudel. Rudel ohne Spieler im Umkreis von 160 m
  verschwinden.
- **FPS:** höchstens 3 Rudel pro Spieler, keine zusätzlichen Lichter. Gemessen: 76–92 FPS im Dorf, in der Arena und
  im Lager (RTX 4070 SUPER, 1600×900, Qualität „Hoch“, TSR 67 %).

## Dämonenlager und Hauptmann

`ADBDemonCamp`: ein Lager je Vasallengebiet, gegenüber der Arena des Vasallen, immer im eigenen Gebiet und auf flachem
Boden (`DBBosses::FindOpenGround`). Ausstattung: verderbtes Torii (Sketchfab), Lagerfeuer mit einem schattenlosen Licht,
Kriegsbanner, verderbte Steinlaternen, Palisade; eingebackene Bäume im Lager werden entfernt.

- Kommt ein Spieler auf 55 m heran, erwachen der **Hauptmann** (`MidBoss_RegionXX`, z. B. „Blutklinge, Hauptmann von
  Akakage“) und drei Wachen. Der Hauptmann ist ein Boss mit 45 % der Lebenspunkte seines Vasallen, dessen
  Erstphasen-Mechaniken und Beschwörung in Phase 2 (`LT_Commander`).
- Sein Sieg trägt den Zwischenboss in den Weltstand ein: Das Gebiet wird **umkämpft**, das Lager ist dauerhaft
  zerschlagen (gespeichert), Banner und Feuer erlöschen.
- Sind 8 s lang keine Spieler im Umkreis von 120 m, ruht das Lager wieder (Hauptmann und Wachen verschwinden).

## Befreiungsquests

Beim ersten Betreten eines Vasallengebiets startet für die Gruppe `RQ_RegionXX` „Befreiung: <Gebiet>“ (gemeinsamer
Fortschritt, Ziele in beliebiger Reihenfolge):

1. 6 Dämonen des Gebiets erschlagen (`Demon_RegionXX`; Wachen zählen mit),
2. das Dämonenlager zerschlagen (Hauptmann),
3. den Vasall besiegen.

Belohnung: Erfahrung, Mon, ein Fähigkeitspunkt, Story-Flag `Story.Liberated.RegionXX`.

## Karte

Dämonenlager als Dreieck (orange, grau wenn zerschlagen), dazu die Bossarenen aus Phase 10.

## Testbefehle

| Befehl | Wirkung |
|---|---|
| `DBRegionDump` | Zustand aller Gebiete (Kontrolle, Einfluss, Rudel pro Spieler, Lager), Rudel in der Welt, Gebiet des Spielers |
| `DBCamp <Nr.\|Name>` | 48 m vor das Lager eines Gebiets springen (es erwacht) |
| `DBRegionPack` | sofort ein Rudel neben dem Spieler (ohne Budget) |
