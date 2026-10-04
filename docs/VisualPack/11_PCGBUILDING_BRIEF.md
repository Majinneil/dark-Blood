# PCG / BUILDING BRIEF

## Ziel
Mit wenigen hochwertigen Modulen viele glaubwürdige Gebäude erzeugen, ohne sichtbare Wiederholung.

## Gebäudeparameter
- footprint size
- floor count
- roof family
- facade module family
- entrance orientation
- veranda yes/no
- lantern density
- settlement wealth
- region style
- damage state

## Gebäudetypen
- kleines Wohnhaus
- großes Wohnhaus
- Händlerhaus
- Schmiede
- Taverne
- Lagerhaus
- Wachhaus
- Tempel-Nebengebäude
- Dorfverwaltung

## Regeln
- keine zufälligen Dächer ohne Statiklogik
- Türen müssen NavMesh-/NPC-zugänglich bleiben
- Fensterhöhen konsistent
- Gameplay-Collision beibehalten
- Schäden als Varianten, nicht destructive chaos everywhere

## Siedlungs-PCG
Input:
- road spline
- buildable polygons
- water exclusion
- slope
- settlement tier

Output:
- buildings
- fences
- lanterns
- carts
- market props
- foliage edge dressing

Handgebaute Hero-Locations bleiben manuell.
