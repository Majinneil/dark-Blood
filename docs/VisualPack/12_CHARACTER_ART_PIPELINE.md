# DARK BLOOD – Character Art Pipeline (UE 5.8)

## Ziel
Menschen in DARK BLOOD sollen realistisch und glaubwürdig wirken: Gesichter, Haut, Haare, Augen, Kleidung, Rüstung und Mimik dürfen nicht nach Greybox oder generischem Mannequin aussehen.

## Primärer kostenloser Human-Stack

### MetaHuman Creator in UE 5.8
Nutze MetaHuman als Basis für:
- Spielercharaktere
- König und Hauptfiguren
- wichtige Quest-NPCs
- Händler, Wachen und Zivilisten über optimierte Varianten

UE 5.8 integriert MetaHuman Creator direkt in Unreal. Der Workflow unterstützt Gesicht, Körper, Haare, Kleidung und Materialien sowie Full Rig/Joints Only Rig und 2K/4K/8K Texturen.

WICHTIG: MetaHuman-Rohinhalte werden nicht in dieser ZIP weiterverteilt. Der Nutzer muss sie im eigenen Unreal-/Epic-Account erzeugen bzw. beziehen.

### Qualitätsstufen
- HERO: Full Rig, höchste Texture Assembly, hochwertige Groom-Haare, volle Gesichtsanimation.
- IMPORTANT NPC: Full Rig oder optimierte Assembly, 2K/4K Texturen.
- CROWD: Joints Only / Crowd-optimierte Assembly, aggressive LODs und reduzierte Face Cost.

## Spieler-Charaktererstellung
Die Runtime-Charaktererstellung soll nicht den MetaHuman Editor nachbauen. Stattdessen:
- vorbereitete Gesichts-/Körper-Presets
- Hauttöne
- Frisuren
- Haarfarben
- Augenfarben
- Narben/Decals
- Bartoptionen
- Outfit-Slots
- Rüstungsslots

Charaktername bleibt vollständig frei wählbar und serverseitig gespeichert.

## Kleidung – 14.-Jahrhundert-inspiriertes Fantasy-Japan
Baue Kleidung modular:
- Untergewand
- Kosode-/Kimono-inspirierte Basisschicht
- Hakama-inspirierte Hosen
- Wickelgürtel/Obi-inspirierte Gürtel
- Reiseumhang
- Strohhut / Kopfbedeckung
- leichte Krieger-Rüstung
- Lamellen-/Samurai-inspirierte Rüstung
- Magiergewänder
- Mönchskleidung
- Händler-/Bauern-/Wachenvarianten

Historische Inspiration ja, aber DARK BLOOD bleibt ein eigenes Fantasy-Reich.

### Kostenlose Produktionsmethode
1. MetaHuman-Körper als Referenz/Target Skeleton.
2. Kleidung in Blender modellieren oder aus rechtlich geeigneten freien Ausgangsmeshes ableiten.
3. Cloth-Teile sauber retopologisieren.
4. Skinning auf Ziel-Skeleton.
5. Weight Paint prüfen.
6. UE Cloth/Chaos nur für relevante Teile verwenden.
7. LODs erstellen.
8. Material Instances pro Stoff/Farbe/Region.

## Haare
Priorität:
- MetaHuman Grooms für Hero-Charaktere.
- Hair Cards / optimierte Grooms für normale NPCs.
- LOD-Ketten und Groom Binding testen.
- Lange Haare dürfen Combat/Helme nicht permanent clippen.

## Gesicht und Mimik
Hero-NPCs benötigen:
- Blickrichtung
- Blinzeln
- Atmung/Idle
- emotionale Grundzustände
- Lippenbewegung
- Brows/Cheeks/Jaw
- Schmerz-/Kampfreaktionen

MetaHuman Animator ist für Gesichts- und Körperperformance vorgesehen. Dialog-Cinematics sollen nicht nur starre Idle-Gesichter zeigen.

## Dämonen
MetaHuman nur für humanoide Dämonen verwenden, wenn sinnvoll. Vasallen und Dämonenkönig bleiben eigene Hero-Character-Pipelines mit individuellem Skeletal Mesh, Rig, Materialien und VFX.

## Dateistruktur
/Game/DarkBlood/Characters/
  Player/
  Heroes/
  NPC/
  Crowd/
  Clothing/
  Hair/
  Materials/
  Textures/
  Rigs/
  Animations/

## Abnahmekriterium
In einem Nahaufnahme-Dialog dürfen Haut, Augen, Haare und Kleidung nicht wie Placeholder wirken. Der Charakter muss bei Gameplay-Kameradistanz und Nahaufnahme überzeugend bleiben.
