# Siedlungssimulation

**Status:** Entwurf (Phase 7). Noch kein Code. Grundlagen aus Phase 1: Weltuhr, Regionszustand mit
zeitschrittunabhängiger Entwicklung, versionierter Welt-Datensatz.

## Ziel

Jede Siedlung simuliert Bevölkerung, Familien, Nahrung, Ressourcen, Wohlstand, Arbeitskräfte, Verteidigung,
Wachen, Handel, Gebäude, Schäden, Bauprojekte, Sicherheit und Dämonenbedrohung. Die Welt entwickelt sich
auch ohne Spieler weiter.

## Simulationsstufen

| Stufe | Bedingung | Umsetzung |
|---|---|---|
| 1 – ACTIVE | Spieler in der Nähe, gestreamt | volle NPC-Actors (KI via StateTree, Animation, Kampf, Arbeit), Mass-Entities für Menge |
| 2 – REDUCED | gestreamt, weiter entfernt | reduzierte Tickrate (AI Significance), vereinfachte Animation (Animation Budget Allocator) |
| 3 – ABSTRACT | nicht gestreamt | reine Mathematik im Regelkern (`DarkBloodRules`), z. B. alle 10 Spielminuten |

Übergänge: Beim Aktivieren werden NPCs aus dem abstrakten Zustand materialisiert (Anzahl, Berufe, Tagesplan →
Spawn am plausiblen Ort). Beim Deaktivieren wird der Zustand zurückgeschrieben. Die abstrakte Simulation ist die
einzige Wahrheit für Zahlen; aktive NPCs sind eine Darstellung davon.

## Datenmodell (Entwurf, Regelkern)

```
FSettlementState { Id, RegionId, Population{ Kinder, Erwachsene, Alte }, Families, Guards,
                   Stocks{ Nahrung, Holz, Stein, Erz, Geld }, Prosperity, Security, Threat,
                   Buildings[]{ Typ, Zustand 0..1, Stufe }, Projects[]{ Typ, Fortschritt, Bedarf, Arbeiter },
                   bStoryProtected }
FSettlementRules  { Geburten/Sterberaten, Verbrauch, Produktion je Beruf, Baukosten, Angriffsmodell }
AdvanceSettlement(State, GameHours, RegionDemonInfluence, Rules) -> Events[]  (deterministisch, getestet)
```

Einzel-NPCs mit Beziehungen/Altern/Berufen werden **abstrahiert** (Kohorten + benannte Schlüssel-NPCs). Nur
benannte NPCs (Questgeber, Händler, Familienmitglieder mit Story-Bezug) werden individuell geführt.

## Dämonenangriffe

Wahrscheinlichkeit aus Regions-Dämoneneinfluss, Nacht, Sicherheit und Wachen. Ausgang abstrakt
(Verluste Wachen/Bewohner, Gebäudeschäden) oder aktiv als Encounter, wenn Spieler anwesend sind.
Story-geschützte Orte (`bStoryProtected`) können beschädigt, aber nie zerstört oder entvölkert werden, damit
keine Story-Blockaden entstehen.

## Befreiung (Verknüpfung mit Phase 1)

`FWorldState::Advance` senkt den Dämoneneinfluss befreiter Regionen. Die Siedlungssimulation liest diesen Wert:
weniger Angriffe, Rückkehr von Flüchtlingen, Handel wächst, neue Bauprojekte.

## Performance-Ziele

Abstrakte Simulation aller Siedlungen < 1 ms/Server-Frame im Mittel (zeitlich verteilt). Keine Architektur,
die nur mit wenigen NPCs funktioniert: Mass für Menschenmengen, Actors nur für Interaktion.
