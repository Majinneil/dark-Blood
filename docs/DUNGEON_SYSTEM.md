# Dungeon-System

**Status:** Phase 9 ✅ Gameplay – 8 Dungeons mit Toren in der Welt, Innenräume aus Seeds, Raumlogik (Kämpfe, Fallen,
Schatz, Rastschrein, Wächter), Fortschritt gespeichert; Regelkern-, Headless-, Koop- und gerenderte Tests. Offen:
eigene Wächter-Bosse (Phase 10 nutzt das Boss-Framework), Dungeon-Themen mit eigenen Modellen, Rätsel, Respawn im
Dungeon nach dem Tod (derzeit normaler Respawn).

## Regelkern (`DarkBloodRules/Dungeon.h`)

`GenerateDungeon(Seed, Params)` → Raster (40 × 40 Zellen) mit Räumen (3–5 Zellen, zwei Zellen Abstand), Gängen
(minimaler Spannbaum über die Raummitten + `ExtraLinks` Rundwege, L-förmig), Rollen: Eingang nahe der Rasterecke,
Wächterraum mit der größten Tiefe, Schatz in Sackgassen (max. 2), Rastschrein auf halber Tiefe, Fallenräume
(1 + Stufe/2), sonst Kampf. Gegner: Kampf 2 + Stufe/2 + Tiefe/3 (max. 8), Falle 1, Wächterraum 1 + Stufe/2.
`ValidateDungeon` prüft einen Eingang, einen Wächterraum (am tiefsten), keine Überlappung, alle Räume erreichbar.
Fortschritt: `FDungeonProgress` (Anzahl, Zeitpunkt), gesäubert für 72 Spielstunden; im Welt-Datensatz ab Version 3.
Tests: 900 Dungeons gültig, Rollen, Fortschritt + Speichern.

## Spiel

| Dungeon | Gebiet | Stufe |
|---|---|---|
| Verfallener Schrein | Kirschblütental | 1 |
| Bambusgruft | Bambuswälder | 2 |
| Eishöhle | Eisöde | 3 |
| Geistergrotte | Wald der Geister | 3 |
| Wüstengrab | Wüstenlande | 3 |
| Aschestollen | Feuergebirge | 4 |
| Festungskerker | Vasallenfestung | 5 |
| Dämonenschlund | Dämonenöde | 5 |

- **Tor** (`ADBDungeonPortal`): rotes Torii mit Schleier und Namen, auf trockenem Land außerhalb der Siedlungen, zur
  Gebietsmitte ausgerichtet; Weltkarte `[M]` zeigt alle Tore. `[E]` betritt (ein Pferd bleibt draußen).
- **Innenraum** (`ADBDungeonInstance`): vom Server beim ersten Betreten erzeugt, repliziert; jeder Rechner baut ihn
  aus dem Seed (Boden, Wände, Decke, Fackeln; Zellen 6 m, Wände 5,2 m) westlich der Welt über dem Meer. Die Bauteile
  sind netzwerk-adressierbar (Bewegungsbasis im Koop). Neue Innenräume erscheinen beim Client, bevor der Spieler folgt.
- **Räume** wachen auf, wenn ein Spieler eintritt: Kampf → niedere Dämonen (+1 je weiterem Spieler im Dungeon);
  Falle → Feuerplatten im Schachbrett (Warnglühen, dann Ausbruch: 12 % Leben, die andere Hälfte versetzt);
  Schatz → Truhe (`LT_DungeonChest`); Rast → Schrein (volle Heilung); Wächterraum → Wächter + Meldung.
- **Gesäubert**: Wächterraum leer → Fortschritt gespeichert, 120 XP × Stufe für alle im Dungeon, Ausgang und Hort
  (`LT_DungeonGuardian`) erscheinen. Danach bleibt der Dungeon 3 Spieltage leer. Ein ungesäuberter, verlassener
  Innenraum verschwindet nach 5 Minuten (frische Dämonen beim nächsten Mal).
- Außerhalb der Landschaft gelten keine Kälte und keine neue Regionsstimmung (`DBRealm::IsInside`).

## Getestet

Headless: Betreten, Kampfraum (3 Dämonen, Beute, gesäubert), Fallen (2 Treffer, 268 → 174), Schrein (voll geheilt),
Wächter (Meldung, gesäubert, +120 XP, Level 2); nach Neustart gesäubert, Kampfraum bleibt leer, Truhe (Beute),
Ausgang zurück ins Kirschblütental. Koop: Client durch das echte Tor, Innenraum beim Client gebaut, Raumkampf, eigene
Beute je Spieler, keine Netzwerkwarnungen. Gerendert: Tor, Gänge, Kampfraum, Fallen, Wächterraum, 90 FPS.

## Testbefehle

`DBDungeonEnter <Name>`, `DBDungeonRoom <Index|Combat|Trap|Treasure|Rest|Boss|Entrance>`, `DBDungeonDump`,
`DBKillNearby <m>`.
