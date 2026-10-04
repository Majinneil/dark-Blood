# Performance

**Status:** Gemessen im gepackten Development-Build (2026-09-28), Ryzen 7 5800X, RTX 4070 SUPER, 1600×900.
Wunsch des Projekts: mindestens 160 FPS.

## Messungen (gepackt, `DBPerfSnapshot`)

Großstadt in der Abenddämmerung (≈1100 Häuser, Laternen an), Blick über die Straße:

| Preset | FPS | GPU | Render-Thread |
|---|---|---|---|
| Episch + Hardware-Raytracing, nativ | 76 | 10,3 ms | 12,8 ms |
| Hoch + TSR 67 % | 105 | 7,2 ms | 9,3 ms |
| Hoch + TSR 50 % | 106 | 6,9 ms | 9,5 ms |
| Mittel + TSR 67 % | 118 | 6,2 ms | 8,2 ms |
| Niedrig + TSR 67 % | 164 | 4,0 ms | 5,3 ms |

Auf See am Hauptstadthafen (Schiff in Fahrt): Hoch 101 FPS, Niedrig 151 FPS.

Erkenntnisse: Auflösung spielt kaum eine Rolle (TSR 50 % ≈ 67 %); die Kosten liegen in Lumen (GI) und Virtual
Shadow Maps (`sg.ShadowQuality 0` bzw. `sg.GlobalIlluminationQuality 0` einzeln je +15–20 FPS, Zwischenstufen fast
nichts) und im Render-Thread. 160 FPS mit Lumen und Schatten sind auf dieser Hardware nur mit Frame Generation
(DLSS 3 / FSR 3) realistisch; nativ erreicht sie das Niedrig-Preset.

## Budgets (Ziel, zu validieren)

| Ziel | Wert |
|---|---|
| Client HOCH, Referenz-GPU der Mittelklasse | 60 FPS bei 1440p mit TSR |
| Client NIEDRIG | 30+ FPS auf Mindestsystem |
| Server-Frame (4 Spieler, aktive Region) | < 20 ms @ 30 Hz |
| Replikation pro Client | < 30 KB/s im Mittel außerhalb von Bosskämpfen |
| Abstrakte Siedlungssimulation | < 1 ms/Frame im Mittel (verteilt) |

## Bereits umgesetzt (Phase 1)

- Inventar: Fast-Array-Delta-Replikation, nur Besitzer; Sicht wird nur bei Änderungen synchronisiert.
- Weltuhr: Replikation alle 2 s, Clients extrapolieren.
- Regionen: Dämoneneinfluss als Byte quantisiert, Sicht-Sync alle 10 s.
- Sekundärattribute nur an den Besitzer.
- Regelkern ohne Allokationen im Schadenspfad; Datensätze sind kompakt (binär).
- Regionsvolumen sind nicht räumlich gestreamt, aber reine Trigger (keine Kosten außer Overlap-Tests).

## Umgesetzt (offene Welt)

- Materialien mit Nanite- und ISM-Usage-Flags (ohne sie fielen Instanzen auf den klassischen Pfad: 16 → 85 FPS).
- Eigene Nanite-Kopien der Grundformen für das Bau-Kit (`/Game/DarkBlood/Art/Kit/Shapes`).
- Vegetation als ISM statt HISM in 1-km-Zellen.
- Siedlungshäuser in gemeinsamen Instanz-Blöcken (40 Häuser je Block, unregistriert gefüllt, dann registriert),
  über Frames verteilt gebaut (3 ms Budget): Großstadt 13 600 → ~480 Komponenten, kein Ruckler beim Annähern.
- Kleine Punktlichter (Laternen, Häuser) blenden ab 50 m aus: Niedrig in der Großstadt 93 → 164 FPS.

## Geplante Maßnahmen

World Partition + HLOD, Nanite für statische Geometrie, Niagara-Scalability, AI Significance Manager, Animation
Budget Allocator, Mass für Menschenmengen, Simulationsstufen (SETTLEMENT_SIMULATION.md), Replication Graph oder
Iris für große Actorzahlen, Net Dormancy für ruhende Actors, Async-Asset-Loading (Data-Subsystem lädt in
Phase 1 synchron, weil nur kleine Gameplay-Daten betroffen sind).

## Profiling-Plan (Phase 20)

Unreal Insights (CPU/GPU/Speicher/Streaming/Netzwerk), `stat unit/gpu/net`, `memreport`, Network Profiler,
Dedicated-Server-Profiling unter Last (Bots), Grafikpresets je Zielhardware.
