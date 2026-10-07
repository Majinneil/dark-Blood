# Himmel, Atmosphäre und Dämmerungs-Look

Ziel: der Wuxia-Dämmerungs-Look (Referenz: `docs/VisualPack/Reference` / „Where Winds Meet“-Stil) – Akaza über einer
Palaststadt im Tal, brennend rot-violetter Horizont, Täler im Nebel, Gipfel klar, warme Laternen gegen kühle Schatten,
der Blutmond über dem Horizont. Alles ist **voll dynamisch** (Lumen, Virtual Shadow Maps, Echtzeit-Himmelsaufnahme) –
nichts wird gebacken; die Tageszeit des Spiels (`DBWorldStateComponent`) treibt den Himmel.

Code: `UDBRealmSkyComponent` (`Source/DarkBlood/Private/World/DBRealmSkyComponent.cpp`) auf `ADBRealmDirector`. Die
Regionsstimmung (`UDBRealmMoodComponent`) multipliziert Nebel und Sonne weiterhin obendrauf (`SetSkyBase`).
Test: `DBSky 18.33` (Dämmerung), `DBSky 12` (Mittag), `DBSky -1` (wieder der Spieluhr folgen), `DBSetTime <h>`.

## 1. Tageslauf (Schlüssel, weich interpoliert)

| Uhr | Sonne über Horizont | Sonne (Lux, Projektskala) | Farbtemperatur | Skylight | Nebel × | Dämmerung | Mond |
|---|---|---|---|---|---|---|---|
| 0:00 | −40° | 0 | – | 0,5 | 1,2 | 0 | 1 |
| 5:48 | −2° | 3 | 2400 K | 0,8 | 1,7 | 0,5 | 0,6 |
| 12:00 | 58° | 10 | 5800 K | 1,0 | 1,0 | 0 | 0 |
| 17:24 | 7° | 8 | 3300 K | 1,0 | 1,25 | 0,45 | 0,2 |
| **18:20** | **−1,5° (hinter dem Horizont)** | 7 | 2900 K | 1,25 | 1,7 | **1** | 0,85 |
| 19:12 | −4° | 1,5 | 2000 K | 0,8 | 1,6 | 0,75 | 1 |
| 20:30 | −15° | 0 | – | 0,55 | 1,3 | 0,15 | 1 |

Die Sonne geht im Osten (+Y) auf und im Westen (−Y) unter, 15° pro Stunde. Steht sie hinter dem Horizont, färbt sie
nur noch die Atmosphäre (karmesin) – die Szene leben Himmelslicht und Laternen, wie im Referenzbild.

## 2. Post-Process (eigenes ungebundenes Volume, Priorität −0,5)

| Gruppe | Einstellung | Wert (Tag → Dämmerung) |
|---|---|---|
| Belichtung | Auto Exposure Bias | 0 → −0,3 (Nacht −0,3) |
| | Min / Max Brightness (EV100) | −2 / 14; Geschwindigkeit hoch 1,5, runter 1,0 |
| Lokale Belichtung | Highlight / Shadow Contrast Scale | 0,75 / 0,9 (Laternen behalten Zeichnung, Schatten bleiben tief) |
| Tonemapper (ACES) | Slope / Toe / Shoulder / Black / White Clip | 0,86 / 0,6 / 0,3 / 0 / 0,04 |
| Bloom | Intensity / Threshold / Size Scale | 0,35 → 0,75 / −1 (physikalisch) / 4 |
| Kontrast | Color Contrast (global) | 1,0 → 1,1 |
| Schatten (≤ 0,09) | Gain / Saturation | (0,90, 0,93, 1,14) kühl-violett / 0,85 |
| Lichter (≥ 0,45) | Gain / Saturation | (1,05, 0,99, 0,94) warm / 1,15 |
| Film | Grain / Chromatic Aberration | 0 → 0,06 / 0 |
| Region (Mood-Volume, Prio −1) | Saturation, Gain, Vignette | je Region (`GetBiomeMood`) |

## 3. Nebel, Wolken, Atmosphäre (Tiefenstaffelung)

- **Exponential Height Fog, Schicht 1** (Dunst über allem): Dichte 0,012 × Tagesfaktor × Region, Height Falloff 0,35
  (dünnt mit der Höhe schnell aus → Gipfel bleiben klar), Max Opacity 0,97, Start 15 m.
- **Schicht 2** (Talnebel): Dichte 0,02 (Tag) → 0,07 (Dämmerung), Falloff 0,9, Höhe +25 m über dem Meer – Städte und
  Flüsse versinken, Grate ragen heraus.
- **Farbe:** Tag (0,45, 0,5, 0,6), Dämmerung (0,30, 0,25, 0,38) violett-grau, Nacht (0,03, 0,04, 0,08);
  Himmelsbeitrag zum Nebel in der Dämmerung (0,45, 0,36, 0,5) – sonst glüht der ferne Nebel weiß.
- **Gerichtete Streuung** (Sonnenglühen im Nebel): Exponent 6, ab 40 m, Farbe → (0,55, 0,2, 0,16) in der Dämmerung.
- **Volumetrischer Nebel:** Streuverteilung 0,75 (vorwärts – Lichtschächte, Laternen-Halos), Albedo (235, 222, 230),
  Reichweite 3 – 90 m (Bildrate).
- **Sky Atmosphere:** Sky Luminance Factor (1,05, 0,78, 1,15) in der Dämmerung (karmesin-violett statt orange),
  Aerial Perspective Distance Scale 1 → 2,2 (ferne Kämme verblassen schichtweise in den Himmel).
- **Volumetrische Wolken:** Unterkante 2,5 km, Dicke 6 km, Samples × 0,6 (Ansicht) / 0,25 (Reflexion) / 0,5 (Schatten),
  Boden-Verdeckung des Himmelslichts 0,6; die Sonne wirft Wolkenschatten (Stärke 0,6).

## 4. Licht (Lumen, nichts gebacken)

- **Sonne** (Directional Light, Atmosphärenlicht 0): beweglich, Temperatur an, Quellwinkel 1,2°, volumetrische Streuung
  1,6, *kein* Light-Shaft-Bloom (verschmiert am Horizont), Virtual Shadow Maps.
- **Blutmond** (zweites Directional Light, Atmosphärenlicht 1): Quellwinkel 7° (großer Mond), Farbe (1, 0,16, 0,12),
  Scheibe (0,35, 0,025, 0,02) × Sichtbarkeit, Licht 0,8 Lux × Sichtbarkeit, wirft keine Schatten (kostet fast nichts);
  tief im Westen bei Dämmerung, steigt nachts auf 35°.
- **Skylight:** Echtzeit-Aufnahme der Atmosphäre (die Umgebung nimmt zu jeder Stunde die Himmelsfarbe an), untere
  Halbkugel (0,05, 0,035, 0,06), Intensität laut Tabelle.
- **Lumen** (Post-Process): GI und Reflexionen Lumen, Scene Lighting Quality 1, Final Gather Quality 1, Trace-Distanz
  200 m, Skylight Leaking 0,05 (Innenräume nicht stockdunkel).
- **Laternen** (`ADBModularBuilding` / `ADBArtActor`): Punktlichter in Lumen, beweglich, Quellradius 10 – 15 cm, ab
  50 m ausgeblendet (eine Stadt hat Hunderte); ferne Städte leuchten über die emissiven Laternen-Materialien und Bloom.

## 5. Laub: Gegenlicht (Kirschblüten)

`M_DB_Foliage_Master` (Shading *Two Sided Foliage*, alle Blätter und Blüten, auch die Sketchfab-Bäume) hat zwei HLSL-
Knoten (`Tools/UE58/db_patch_foliage_backlight.py`):

```hlsl
// DBBacklight -> Subsurface Color: mehr Durchleuchtung, wenn die Kamera durch das Blatt zur Sonne blickt
float Back = pow(saturate(dot(-CameraVector, SunDirection)), Power);
return Subsurface * (1.0 + Boost * Back);

// DBBacklightGlow -> + Emissive: Randglühen in Farbe und Stärke der gefilterten Sonne (rot in der Dämmerung)
float Back = pow(saturate(dot(-CameraVector, SunDirection)), Power * 0.5);
float Horizon = saturate(1.0 - abs(SunDirection.z) * 2.0);
return Subsurface * Illuminance * Back * Glow * (0.35 + Horizon);
```

`SunDirection` = *SkyAtmosphereLightDirection* (Licht 0), `Illuminance` = *SkyAtmosphereLightIlluminance* – der Effekt
folgt so der Tageszeit ohne Blueprint. Instanz-Parameter: `BacklightPower` 6, `BacklightBoost` 2,5, `BacklightGlow` 0,06.

## Stand und Grenzen

- Gerendert geprüft (offscreen, 1600 × 900): Mittag unverändert, Dämmerung karmesin mit glühenden Gipfeln, Talnebel und
  Gegenlicht-Blüten, Nacht unter dem Blutmond (rot – das DARK-BLOOD-Thema).
- Die **Bildrate** war bei diesen Tests nicht messbar (ein anderes Spiel lief gleichzeitig auf der Grafikkarte).
- Die Referenz-**Komposition** (Palaststadt tief im Tal, Wasserfälle, Klippenpfad) gibt das Gelände der Welt noch nicht
  her – das ist Level-/Weltbau, nicht Licht.
