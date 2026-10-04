# Siedlungssimulation

**Status:** Phase 7 ✅ Gameplay – abstrakte Simulation aller 17 Siedlungen im Regelkern (gespeichert), aktive Stufe mit
Dorfbewohnern und echten Dämonenangriffen, wenn Spieler vor Ort sind; Regelkern-Tests und Headless-Tests. Offen: Mass-
Menschenmengen, StateTree-Tagesabläufe, Animationen, Händler (Phase 7+/Assets).

## Ziel

Jede Siedlung simuliert Bevölkerung, Familien, Nahrung, Ressourcen, Wohlstand, Arbeitskräfte, Verteidigung,
Wachen, Handel, Gebäude, Schäden, Bauprojekte, Sicherheit und Dämonenbedrohung. Die Welt entwickelt sich
auch ohne Spieler weiter.

## Simulationsstufen

| Stufe | Bedingung | Umsetzung |
|---|---|---|
| 1 – ACTIVE | Spieler näher als Radius + 400 m (aus bei + 700 m) | `UDBSettlementLifeComponent` (Server, am `ADBRealmDirector`): Dorfbewohner `ADBVillagerCharacter` (Bevölkerung / 12, 3–20, nachts ein Viertel), wandern durch die Siedlung, erzählen vom Zustand; Angriffe der Simulation werden zu Kämpfen (2 + Spieler, max. 6 niedere Dämonen) mit Meldung |
| 2 – REDUCED | – | noch nicht nötig (wenige Actors); später AI Significance / Animation Budget |
| 3 – ABSTRACT | immer | `DarkBloodRules/Settlement.h`: ganze Spielstunden, deterministisch, auf der Weltuhr |

Die abstrakte Simulation ist die einzige Wahrheit für Zahlen; Bewohner sind eine Darstellung davon.

## Regelkern

```
FSettlementState { Id, RegionId, Children/Adults/Elders, Guards, Stocks{Food, Wood, Stone, Ore, Money},
                   Prosperity, Security, Threat, Buildings[]{Houses, Farms, Workshops, Market, Walls, Barracks, Temple:
                   Condition 0..1, Level}, Projects[]{Repair|Upgrade, Target, Progress, WorkNeeded}, bStoryProtected,
                   SimulatedHours, Seed, HungryHours }
AdvanceSettlement(State, GameHours, DemonInfluence, StartTimeOfDay, Rules) -> Events[]
FWorldState::Settlements / AddSettlement / AdvanceSettlements (Uhr der Welt, Einfluss der Region)
```

Pro Stunde: Produktion (Feldarbeit 55 % der Arbeiter × 4,5 Nahrung/Tag, Handwerk Holz/Stein/Erz, Handel nach Wohlstand
und Markt), Verbrauch (1 Nahrung/Person/Tag), Hunger, Geburten (mit Nahrung), Altern, Todesfälle, Hungertod nach 24 h
ohne Nahrung, Bedrohung = Einfluss × (1 − 0,3 × Mauern), Sicherheit aus Wachenquote (8 %), Mauern, Kaserne, Angriffe
(nachts 0,08 × Bedrohung × (1 − 0,8 × Sicherheit), tagsüber 15 % davon): abgewehrt (Wachen fallen) oder Schaden an einem
Gebäude, Opfer, geplünderte Vorräte. Täglich: Wachen anwerben (6 Uhr), Bauprojekt starten (7 Uhr; Reparatur vor
Ausbau: Mauern unter Bedrohung, Felder bei Nahrungsmangel, sonst Häuser/Markt), Flüchtlinge kehren in befreite, sichere
Regionen zurück (12 Uhr). Zufall aus Siedlungs-Seed + Stundenindex → ein 48-h-Schritt = 48 Einzelstunden.

Balance (getestet): besetzte Siedlung (Bedrohung ~0,85) kommt knapp über die Runden, befreite wächst mit Überschuss.

## Story-Schutz

`bStoryProtected` (Hauptstadt, Hauptstadthafen): nie unter 12 Bewohner, Gebäude nie unter 25 % Zustand.

## Speicherung

`WorldRecordVersion = 2` hängt die Siedlungen an den Welt-Datensatz; Version-1-Welten laden ohne Siedlungen, das
Spiel legt sie an (`UDBWorldStateComponent::EnsureSettlementsRegistered`, Startbewohner nach Siedlungstyp).
Clients erhalten `FDBSettlementView` (Bevölkerung, Wachen, Nahrungstage, Wohlstand/Sicherheit/Bedrohung, schlechtester
Gebäudezustand, Hunger, letzter Angriff).

## Testbefehle

`DBDumpSettlements`, `DBSkipHours <h>`, `DBSettlementAttack <Siedlung>`, `DBGoto Villager` + `[E]`.

## Performance

Eine Stunde aller 17 Siedlungen kostet Mikrosekunden; simuliert wird alle 2 Echtminuten (1 Spielstunde). Aktive Stufe:
höchstens 20 Bewohner je Siedlung in Spielernähe.
