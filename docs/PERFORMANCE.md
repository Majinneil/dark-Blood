# Performance

**Status:** Keine Messungen (kein UE-Build möglich). Dieses Dokument legt Budgets und die bereits getroffenen
Designentscheidungen fest.

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

## Geplante Maßnahmen

World Partition + HLOD, Nanite für statische Geometrie, Niagara-Scalability, AI Significance Manager, Animation
Budget Allocator, Mass für Menschenmengen, Simulationsstufen (SETTLEMENT_SIMULATION.md), Replication Graph oder
Iris für große Actorzahlen, Net Dormancy für ruhende Actors, Async-Asset-Loading (Data-Subsystem lädt in
Phase 1 synchron, weil nur kleine Gameplay-Daten betroffen sind).

## Profiling-Plan (Phase 20)

Unreal Insights (CPU/GPU/Speicher/Streaming/Netzwerk), `stat unit/gpu/net`, `memreport`, Network Profiler,
Dedicated-Server-Profiling unter Last (Bots), Grafikpresets je Zielhardware.
