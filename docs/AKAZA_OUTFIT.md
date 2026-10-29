# Akazas Kleidung (Blender)

Akaza Kurosaki trägt nach der Vorlage des Projektinhabers einen cremefarbenen, dicht bedruckten Kimono mit weiten,
hängenden Ärmeln und schwarzen Saumborten, darunter ein schwarzes Wickelgewand mit gemusterter Saumborte, weite
schwarze Hosen mit Seilwicklung an den Schienbeinen, Lederstiefel, einen schweren schwarzen Kapuzenschal, eine Stola
über der linken Schulter, Obi mit zwei Ledergürteln, Schnalle, hängendem Riemen mit Ring, cremefarbene Quaste,
diagonalen Lederriemen und Unterarmwicklungen. MetaHuman bringt keine passende Kleidung mit, Fab-Angebote sind
kostenpflichtig oder für KI gesperrt – deshalb entsteht alles prozedural, ohne fremde Assets und ohne Lizenzfragen.

## Stoffe – `Tools/Art/db_make_kimono_fabrics.py`

`python -I Tools/Art/db_make_kimono_fabrics.py [Ordner] [Größe]` schreibt kachelbare Sätze (BaseColor, Normal,
Roughness, 2048 px) nach `SourceArt/Generated/AkazaOutfit` (lokal, ignoriert, jederzeit neu erzeugbar):

| Satz | Verwendung |
|---|---|
| `Fabric_IvoryPattern` | Kimono: Creme, dichter Blockdruck (Kirschblüten, Chrysanthemen, Tomoe, Blätter, Rankenfelder), Doppellinien, verblasste Färbung |
| `Fabric_BlackMotif` | Wickelgewand, Stola: schwarze Seide mit verblassten Wappen |
| `Fabric_BlackBorder` | Saumborte (kachelt seitlich): Karakusa-Ranke mit Spiralen und Blättern zwischen Doppellinien, Wellenband |
| `Fabric_BlackPlain` | Hosen, Schal, Obi, Kragen, Wicklungen |
| `Leather_Brown` | Gürtel, Riemen, Stiefel (dunkel getönt) |
| `Rope_Hemp` | Seilwicklungen |

Das feine Garn liegt nicht in den Grundtexturen (bei 2048 px auf 85 cm würde es als Raster sichtbar); in Unreal kommt
es als fein gekachelte Detail-Normal-Map in den Stoffmaterialien dazu.

## Gewand – `Tools/Art/blender_make_akaza_outfit.py`

`blender -b --python Tools/Art/blender_make_akaza_outfit.py -- [Körper.fbx] [Ausgabe] [Stufe]`

- **Körper:** ein gehäuteter Körper auf dem UE5-Skelett – Akazas MetaHuman-Körper aus Unreal oder (Standard, solange
  der MetaHuman-Build fehlt) der Mannequin `SourceArt/Mannequin/SKM_Manny_Simple.fbx`. Alle Maße werden per Strahl
  von den Knochenachsen gemessen; das Skript passt sich also an jeden Körper an.
- **Aufbau:** Kimono-Rumpf aus gemessenen Querschnitten (blusig über dem Obi, unten gerade fallend), Schulterpasse
  über die Schultern gelegt; die Armlöcher werden entlang des Oberarms zu tiefen Kimonoärmeln ausgeformt, die letzten
  12 cm tragen die Borte. Wickelgewand mit Überlappung vorn, Falten, immer frei von Hüften und Knien. Hosen,
  Stiefel (auf den Fuß geschrumpftes, geglättetes Volumen), Bänder und Riemen liegen auf der jeweils äußeren Fläche.
- **Stoffsimulation (Blender Cloth):** Ärmel (Rumpf gepinnt), Wickelgewand (Bund gepinnt), Stola (Schulter gepinnt)
  und Schal (Halsring gepinnt, länger zugeschnitten als der Weg zur Schulter, mit Eigenkollision – der Überschuss legt
  sich in schwere Falten, hinten als liegende Kapuze). Vorher wird jede Ecke aus dem Körper geschoben; läuft der Löser
  trotzdem davon (> 35 cm Bewegung), bleibt die unsimulierte Form und das Protokoll warnt.
- **Gewichte:** Wo der Stoff am Körper anliegt, vom Körper übertragen (nächste Fläche, interpoliert); wo er frei hängt
  (Ärmel, Wickelgewand, Riemen, Quaste, Stola unten), nach Abstand zu den Knochensegmenten des jeweiligen Arms bzw.
  Beckens – ein Wickelgewand folgt mit wachsendem Abstand zu den Beinen dem Becken, damit ein Schritt den Saum schwingt
  statt ihn zu dehnen. Höchstens vier Einflüsse, normalisiert.
- **Ausgabe:** `SK_Akaza_Outfit.fbx` (ein Mesh, Materialplätze `MI_Akaza_*`, das Skelett des Körpers),
  `Akaza_Outfit.blend` und Prüfbilder vorn, Seite, hinten, schräg.
- **Stufen** zum Prüfen: `kimono_raw`, `kimono`, `lower` – bauen bis dahin, rendern, enden.

Stand mit dem Mannequin: rund 29 000 Ecken, 76 000 Dreiecke (Schal und Stola einmal unterteilt), alle Ecken gewichtet,
Laufzeit etwa 1,5 Minuten.

Der Export rechnet in **Zentimetern** (Szene ×100, Einheitenmaßstab 0,01): Mit Metern und „FBX Units Scale“ kam das
Mesh in Zentimetern, die Knochen aber in Metern an – das Gewand verschwand im Spiel auf Knochengröße. Dasselbe gilt
für `Tools/UE58/blender_rig_hyper3d.py`.

## In Unreal – `Tools/UE58/db_import_akaza_outfit.py`

`UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_import_akaza_outfit.py`

- `M_DB_Cloth_Master` (`/Game/DarkBlood/Art/Materials/Master`): Grundfarbe mit Tönung, Normal plus 40-fach gekachelte
  Garnstruktur (`T_Weave_N`), Rauheit mit Untergrenze, wenig Spiegelung für Stoff (`Cloth`), Metallic für Messing.
  Default Lit – das Cloth-Shading-Modell über Material-Attribute blieb im Spiel (UE 5.8, Substrate) grau.
- Texturen nach `/Game/DarkBlood/Characters/Akaza/Outfit/Textures`, Instanzen `MI_Akaza_*` (Namen = Materialplätze
  im FBX), `SK_Akaza_Outfit` auf `SK_Mannequin`.
- Import mit dem klassischen FBX-Importer: Interchange las die Bind-Pose der Blender-Datei widersprüchlich.
- Materialplätze: Unreals Python liefert Kopien der Slot-Strukturen – die Liste wird neu gebaut und als Ganzes
  zurückgeschrieben (sonst blieb das Grid-Material).
- Im Spiel (`DBDevelopmentContent`): `CV_MH_Akaza` trägt das Gewand als Teil, das per Leader Pose dem animierten
  Skelett folgt; `CV_AkazaOutfit` zeigt es auf dem Mannequin – `DBSpawnNpc AkazaOutfit Akaza` stellt die Figur vor die
  Kamera.

**Getestet (2026-10-29):** verdeckt gerendert im Spiel (`-RenderOffscreen`): Gewand mit allen neun Materialien auf
der Testfigur in der Ruhe-Animation, von vorn, schräg und hinten; zwei Figuren gleichzeitig. Nicht getestet: Laufen,
Kampf, FPS (Messung nur, wenn am Rechner nicht gespielt wird).

## Offen

- Ärmel liegen bei gesenkten Armen bauschig am Oberarm statt zu hängen – Chaos Cloth für Ärmel, Wickelgewand, Stola,
  Quaste; das Creme wirkt in starker Sonne zu hell (Tönung/Belichtung prüfen).
- Auf Akazas MetaHuman-Körper neu anpassen, sobald sein Build steht (er braucht über 160 GB Speicher, siehe
  [METAHUMANS.md](METAHUMANS.md)); bis dahin trägt die Spielfigur weiter den Hyper3D-Körper.
- Chaos Cloth für Ärmel, Wickelgewand, Stola und Quaste (die Gewichte sind die Grundlage, die Simulation bewegt den
  Rest), verdeckte Körperteile ausblenden.
- Lockiger Haarknoten (Groom) und Narbe über der linken Braue.
