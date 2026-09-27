# CLAUDE CODE – DARK BLOOD VISUAL + CHARACTER + ANIMATION TASK

Lies zuerst alle Dateien 00 bis 17 dieses Pakets.

Du arbeitest im bestehenden DARK-BLOOD-Projekt auf Unreal Engine 5.8. Gameplay bis Phase 5 existiert bereits. Deine Aufgabe ist, die Visual Foundation so vorzubereiten, dass die Greybox schrittweise in ein hochwertiges realistisches Dark-Fantasy-Spiel überführt werden kann.

## Harte Regeln
- bestehendes Gameplay nicht neu schreiben
- Multiplayer nicht beschädigen
- Save/Quest/Combat nicht brechen
- keine Marketplace-Rohassets in Repository kopieren, wenn Lizenz dies nicht erlaubt
- NO-AI-Assets nicht automatisiert analysieren
- MetaHuman/Game Animation Sample nur verwenden, nachdem sie vom Nutzer legitim im Projekt/Epic-Konto bezogen wurden
- jede Greybox-Ersetzung reversibel machen

## Character Foundation
Bereite vor:
- /Game/DarkBlood/Characters/... Struktur
- DB Character Visual Data Asset
- Slots für Face, Hair, Beard, Outfit, Armor, Accessories
- Character Presets für Runtime-Customization
- Serverpersistenz nur über vorhandene Character Data, Visual IDs replizieren
- keine Runtime-Abhängigkeit von Editor-only MetaHuman Tools

Wenn MetaHuman Assets im Projekt vorhanden sind:
- erkenne sie
- integriere sie hinter der vorhandenen Character-Abstraktion
- erstelle LOD-/Quality-Strategie
- richte Player/Hero/NPC/Crowd-Profile ein

## Animation Foundation
Wenn Game Animation Sample Assets im Projekt vorhanden sind:
- nicht blind kopieren
- passende Animation/PoseSearch-Komponenten migrieren
- Motion Matching für Exploration Locomotion aufsetzen
- DARK-BLOOD Combat State davon trennen

Erstelle/überarbeite:
- locomotion state
- combat locomotion
- dodge
- dash
- jump/double jump
- landing
- turn-in-place
- hit reactions
- death/respawn
- montage/event hooks
- Motion Warping points
- IK/foot placement

## Facial Foundation
Für Hero Characters:
- Face animation hooks
- dialogue emotion states
- blink/look-at
- MetaHuman Animator compatible path

## Building/Environment Foundation
Nutze 14_BUILDING_ART_RECIPE.md und bestehende PCG-Briefings.
Baue zuerst nur den Visual Slice:
- Capital street
- village
- forest
- shrine/temple
- tavern interior
- dungeon entrance

## Material Foundation
Nutze 15_TEXTURE_MATERIAL_RECIPE.md.
Keine Asset-Store-Collage. Alle Inhalte über gemeinsame Material-/Color-Grading-/Scale-Regeln vereinheitlichen.

## Prüfung
Am Ende:
- Editor Build
- C++ Build
- PIE
- Multiplayer PIE mindestens 2 Clients soweit möglich
- Asset References auf Missing Files prüfen
- Performance Snapshot erstellen

Erstelle anschließend VISUAL_FOUNDATION_STATUS.md mit:
- integriert
- vorbereitet
- fehlt wegen Asset
- manuell nötig
- Lizenz/AI-Blocker
- nächste konkrete Schritte
