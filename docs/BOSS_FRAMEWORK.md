# Boss-Framework

**Status:** Entwurf (Phase 10). Vorhanden: Bosssiege im Weltstatus (`NotifyBossDefeated`, idempotent,
Zwischenboss → umkämpft, Vasall → befreit, Dämonenkönig → Story-Flag), Multicast-Ereignis `OnBossDefeated`.

## Bausteine (Plan)

| Baustein | Beschreibung |
|---|---|
| `UDBBossDefinition` (Data Asset) | Boss-ID, Rang (WorldBoss/MidBoss/Vassal/DemonKing), Region, Phasen, Arena, Musik, Loot-Tabelle mit garantierten Drops, Fähigkeitspunkte, Story-Events, **Visual-Profil als weiche Referenz** |
| `ADBBossArena` | Arena-Grenzen, Eingang schließen/öffnen, Checkpoint, Respawn-Regeln, Kamera-Hinweise |
| `UDBBossPhaseComponent` | Phasenwechsel nach HP-Schwellen/Zeit/Ereignis, Phasenübergänge als Sequenzen, Wechsel des AbilitySets |
| Boss-KI | StateTree pro Boss, Fähigkeiten als GAS-Abilities, Telegraphing über Anim-Notifies + VFX |
| Bossbar | repliziert: Name, Titel, Phase, Leben; mehrere Bossbalken möglich |
| Intro/Finisher | Level Sequences, Motion Warping für synchronisierte Finisher |

## Multiplayer-Skalierung

Nicht nur „mehr Spieler = mehr HP“. Skalierungsprofil pro Boss:

- zusätzliche Angriffe/Mechaniken ab 2/3/4 Spielern
- Adds (Anzahl nach Spielerzahl)
- veränderte Muster, z. B. geteilte Aufmerksamkeit oder Flächenangriffe
- Zielprioritäten (Aggro, Heiler, isolierte Spieler)
- moderate HP-Skalierung als letzter Regler

## Trennung Gameplay/Visual

Boss-Logik kennt kein Mesh. Das Visual-Profil (Skeletal Mesh, AnimBP, Materialien, VFX, Sounds, Stimme) wird
per ID zugeordnet und kann ausgetauscht werden. Hitboxen kommen aus Sockets/Physics Asset des aktuellen Visuals.

## 14 Vasallen + Dämonenkönig

Jeder Vasall bekommt ein eigenes Definitions-Asset, eigene Abilities, Arena und Musik. Keine Reskins. Der
Dämonenkönig nutzt dasselbe Framework mit vier Phasen (Schwertmeister → Dunkles Blut → Dämonenform →
Arena-Transformation) und löst am Ende das Story-Ende aus.
