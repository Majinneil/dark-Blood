# DARK BLOOD – CLAUDE CODE ASSET-INTEGRATION MASTER TASK

Du arbeitest direkt im bestehenden Unreal-Engine-5.8-Projekt DARK BLOOD.

Das Gameplay ist bereits bis ungefähr Phase 5 entwickelt. Die Welt besteht noch weitgehend aus Greybox/Blockout-Geometrie und Platzhalter-Materialien. Deine Aufgabe ist jetzt **Phase 5.5 – Visual Foundation**.

## Ziel

Ersetze die rein technischen Blockouts schrittweise durch eine hochwertige, realistische bis semi-realistische Dark-Fantasy-Optik mit mittelalterlich-japanischer Formsprache.

DARK BLOOD soll wie ein eigenständiges Third-Person-Action-RPG aussehen – nicht wie ein Prototyp und nicht wie Minecraft.

## Regeln

1. Bestehende Gameplay-, Multiplayer-, Quest-, Combat-, Save- und Inventarsysteme NICHT unnötig neu schreiben.
2. Keine Greybox löschen, bevor ein Ersatz verifiziert ist.
3. Jede Änderung muss reversibel sein.
4. Für große Weltbereiche World Partition, Data Layers, HLOD und PCG berücksichtigen.
5. Nanite für geeignete statische Geometrie nutzen.
6. Lumen/VSM nur visuell verwenden; Gameplay darf nicht davon abhängen.
7. Assets aus `02_ASSET_MANIFEST_AI_SAFE.csv` dürfen automatisiert verarbeitet werden, sofern die aktuelle Lizenz beim Download weiterhin passt.
8. Assets aus `03_MANUAL_ONLY_HIGH_END.md` NICHT an AI-Tools weiterreichen oder automatisiert analysieren. Dafür nur Platzhalter-Slots und dokumentierte manuelle Einbaupunkte erstellen.
9. Keine Stil-Mischung aus Cartoon, Low-Poly und Photorealismus.
10. Zielstil aus `04_VISUAL_BIBLE.md` ist verbindlich.

## Aufgabe A – Projektstruktur

Lege – falls noch nicht vorhanden – diese Struktur an:

/Game/DarkBlood/Art/
  Environment/
    Landscape/
    Rocks/
    Foliage/
    Water/
    Roads/
  Architecture/
    Capital/
    Villages/
    Temples/
    Shrines/
    Bridges/
    Props/
  Materials/
    Master/
    Landscape/
    Architecture/
    Organic/
    DarkBlood/
  VFX/
  Lighting/
  PCG/
  Dev/

## Aufgabe B – Master Materials

Erstelle ein konsistentes Materialsystem mit wiederverwendbaren Master Materials:

- M_DB_Landscape_Master
- M_DB_Wood_Master
- M_DB_Stone_Master
- M_DB_Metal_Master
- M_DB_Foliage_Master
- M_DB_Decal_Master
- M_DB_DarkBlood_Master

Unterstütze soweit sinnvoll:
- Base Color
- Normal
- Roughness
- AO/ORM
- Height/Displacement-Workflow
- Macro Variation
- Detail Normal
- Wetness
- Moss/Grime Masks
- Vertex Painting
- World-aligned Blend
- Material Instances

## Aufgabe C – Landscape

Erstelle ein Landscape-Material, das mindestens unterstützt:

- Waldboden
- verdichtete Erde
- Steinpfad
- felsiger Boden
- Schlamm
- Moos
- Asche/verdorbener Boden
- Schnee (für Frostregion später)

Verwende RVT/Virtual Texturing nur, wenn es für das aktuelle Projekt sinnvoll und stabil ist.

## Aufgabe D – Vegetation/PCG

Baue PCG-Graphen für:

- gemäßigten Wald
- japanisch inspirierte Blütenhaine
- Bergwald
- feuchten Wald
- Bambusbereich als separaten Slot
- Straßenränder
- Tempel-/Schreinumfeld

PCG muss berücksichtigen:
- Slope
- Height
- Distance to road
- Distance to settlement
- Biome/Region tags
- exclusion volumes
- density falloff

Keine Vegetation in Gebäuden oder auf Hauptwegen.

## Aufgabe E – Architektur-Kit

Erstelle ein modulares Architektur-System, das mit austauschbaren Bauteilen arbeiten kann:

- Sockel/Fundament
- Holzstützen
- Wandfelder
- Shoji-/Papierwand-Slots
- Fenster
- Schiebetüren
- Dächer
- Dachkanten
- Veranda
- Treppen
- Zäune
- Torii-/Tor-Slots
- Brücken-Slots
- Laternen-Slots

Wichtig: Nicht jedes Gebäude soll ein Einzelmesh sein. Ziel ist ein modularer Baukasten, der für Dorf, Markt, Taverne, Schmiede, Tempel und Hauptstadt skaliert.

## Aufgabe F – Greybox Replacement

Beginne NICHT sofort mit der gesamten Welt.

Erstelle zuerst einen hochwertigen Vertical Visual Slice:

1. ein Abschnitt der Hauptstadt
2. eine Straße außerhalb der Hauptstadt
3. ein kleines Dorf
4. ein Waldstück
5. ein Bach/Flussabschnitt
6. ein Dungeon-Eingang
7. ein Schrein/Tempelbereich

Die vorhandenen Gameplay-Trigger, NPC-Spawns, Questmarker und Navigation dürfen dabei nicht zerstört werden.

## Aufgabe G – Lighting

Erstelle eine filmische, aber spielbare Beleuchtungsbasis:

- warme Laternen/Stadtlichter
- kühles Mondlicht nachts
- volumetrischer Nebel in Wald-/Dämonengebieten
- klare Lesbarkeit im Combat
- keine übertriebene Bloom-Orgie
- Innenräume mit klarer Lichtführung

## Aufgabe H – Dark Blood Visual Language

Erstelle eine visuelle Sprache für verdorbene Regionen:

- dunkle rote/schwarze Materialvarianten
- pulsierende Emissive-Adern
- dezente Bodenrisse
- Nebel/VFX
- abgestorbene Vegetation
- dunklere Wetness/Soil-Varianten

Nicht alles permanent rot färben. Dunkles Blut soll gezielt und hochwertig wirken.

## Aufgabe I – Performance

Nach jedem Visual-Schritt prüfen:

- GPU Frametime
- Draw Calls
- Nanite usage
- foliage density
- shadow cost
- Lumen cost
- texture pool / VRAM
- HLOD behavior
- World Partition streaming

Ziel: Ultra-Look skalierbar machen, nicht nur einen Screenshot bauen.

## Ergebnis Phase 5.5

Am Ende muss ein klar begrenzter Teil von DARK BLOOD bereits wie das finale Spiel wirken.

Erstelle danach einen Bericht mit:

- importierte/integrierte Assets
- verwendete Quellen
- Materialsystem
- PCG-Systeme
- ersetzte Greybox-Bereiche
- Performance
- offene Asset-Slots
- NO-AI-Manualliste
- nächste Schritte

Keine weiteren Gameplay-Phasen beginnen, bevor Phase 5.5 visuell stabil ist.
