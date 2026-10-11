# Audio (Phase 18)

Alles, was DARK BLOOD hörbar macht. Präsentation: Kein Gameplay wartet auf einen Klang, ein dedizierter Server spielt
nichts ab.

## Soundbank

- Quellen (alle CC0, siehe [CREDITS.md](CREDITS.md)) → `Tools/Audio/db_prepare_audio.py <kenney> <opengameart>` →
  `SourceArt/Audio/<Kategorie>/<Name>_NN.wav` (lokal): 48 kHz 16 bit, One-Shots mono, ohne Stille, Spitze −3 dBFS;
  Loops stereo, Ende ins Anfangsstück überblendet (nahtlos), Lautheit per RMS angeglichen.
- Wasserfall-Rauschen, Bergwind und das Dröhnen der Dämonenlande sind synthetisiert (Rauschen im Frequenzraum gefiltert,
  dadurch selbst nahtlos).
- `Tools/UE58/db_import_audio.py` → `/Game/DarkBlood/Audio/<Kategorie>/S_<Name>_NN` (181 SoundWaves, 33 Klänge, 91 MB).
  Umgebung und Musik loopen; jede Welle hat die Engine-Klangklasse ihrer Art (Music, Voice, sonst SFX).

| Kategorie | Klänge |
|---|---|
| Combat | Swing (16), HitFlesh, HitHeavy, HitClaw, Block, Parry, Stagger, GroundBlast, Unsheathe |
| Voice | DemonGrowl (15), DemonDeath (15), BossRoar (10) |
| Footsteps | Grass, Stone, Snow, Wood, Dirt |
| UI | Notify, Click, Open, Close, Error, Coins, Equip |
| Ambience | ForestDay, Night, Shore, Waterfall, Wind, DemonLands |
| Music | Explore, Battle, Boss |

## Laufzeit: `UDBAudioSubsystem`

- **Abspielen:** `PlayAt(Kontext, "Combat/Block", Ort, Reichweite)` wählt zufällig eine Variation (nie zweimal dieselbe
  hintereinander), streut Tonhöhe ±6 % und Lautstärke −12 %. Reichweiten: Near 15 m, Combat 40 m, Voice 70 m,
  Far 120 m, mit Höhenverlust über die Entfernung (Tiefpass). Pro Klang höchstens 6 Stimmen (Schritte 10, Stimmen 3),
  die fernsten weichen zuerst.
- **Kampf:** Die GameplayCues der Treffer (Phase 17) spielen Klang und Effekt; neu ist der Cue `GameplayCue.Combat.Swing`
  aus jedem Nahkampfschlag (Dämonen knurren dabei manchmal). Parade: hallender Stahl, weit hörbar. Bosse brüllen beim
  Erscheinen (Echos höher und dünner), ihr Fall dröhnt.
- **Schritte** (`UDBFootstepComponent` an jedem Charakter): Schrittlänge nach Tempo und Größe; Untergrund aus den
  Mal-Schichten der Landschaft (Wiese/Wald → Gras, Fels → Stein, Schnee, Sand/Erde/Verderbnis → Erde), Holz an Deck,
  Stein auf Bauwerken; Landen nach Sprüngen; nur im Umkreis von 25 m um den Hörer.
- **Umgebung:** ein Grund-Klang je Gebiet und Tageszeit (grüne Gebiete: Vögel am Tag, Grillen nachts; Gebirge,
  Eis, Wüste, Himmelstempel: Wind; Dämonenland: Dröhnen), dazu eine Schicht (Brandung am Meer; in einem Gebiet unter
  starkem Dämoneneinfluss das Dröhnen). Dungeons, Abgrund, Halle der Echos und Paradies haben ihre eigenen. Wechsel
  blenden über 3 s. Wasserfälle rauschen am Ort (bis 160 m hörbar).
- **Musik:** Erkundung kommt und geht (ein Thema, dann 90–180 s Stille); Dämonen im Umkreis von 15 m → Kampfmusik
  (bleibt 6 s nach dem letzten); ein Boss im Umkreis von 60 m → Boss-Thema. Überblendung 2,5 s.
- **Lautstärke:** Einstellungen „Grafik und Klang“: Gesamt, Musik, Effekte und Umgebung, Stimmen (in 10-%-Schritten),
  sofort wirksam über einen Sound-Mix auf die Klangklassen.

## Testen

- `-DBMute`: echtes Audiogerät, aber stumm (Tests auf einem Rechner, an dem gespielt wird); `-LogCmds="LogDarkBlood
  Verbose"` protokolliert jedes abgespielte Geräusch (`DBAUDIO play ...`).
- Testbefehle: `DBAudio` (Bank, Umgebung, Musik), `DBMusic <0–3|-1>`, `DBSound <Kategorie/Name>`.

**Getestet (2026-10-11):** gerendert-verdeckt mit stummem Audiogerät: Bank 33/181, Umgebung Wald → Wind + Brandung
(Halle), Musik Erkundung → Boss → Kampf → Erkundung, Kampf in der Halle mit Schwüngen, Klauentreffern, Haltungsbruch,
Einschlägen, Boss-Brüllen, Knurren und Todesschrei, Schritte auf Stein; erzwungene Speicherbereinigung ohne Absturz
(behoben: geladene Wellen waren für den GC unsichtbar).

## Offen

- Sprachausgabe für Dialoge, eigene Musik je Region und für den Dämonenkönig (die vorhandenen CC0-Stücke sind ein
  Anfang), Hall in Dungeons (Submix-Effekte), Wetterklänge.
