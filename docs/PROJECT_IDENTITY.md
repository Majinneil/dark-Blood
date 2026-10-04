# DARK BLOOD — verbindliche Projektidentität

"DARK BLOOD is a standalone Unreal Engine 5.8 game.
It is NOT a Minecraft mod and contains no Minecraft technology."

Diese Identität gilt für alle zukünftigen Entwicklungsaufträge in diesem Repository.

- Projektwurzel: `C:\Projekte\dark-Blood`
- Projekteinstieg: `DarkBlood.uproject`, EngineAssociation `5.8`
- Technik: Unreal Engine 5.8, C++, Blueprints und Unreal-Systeme.
- Spiel: eigenständiges Third-Person-Open-World-Dark-Fantasy-Action-RPG mit Koop-Multiplayer.
- 3D-Produktion: Concept Art → Blender → Game-Ready Mesh → Retopology → UV → PBR-Texturen → Rig → Skinning → Animation → Unreal-Import → Blueprint-/C++-Integration.

Minecraft, NeoForge, Forge/Fabric als Modding-Technologien, Java-Modding, Gradle-Modprojekte,
Minecraft-Skins, Resource Packs und Datapacks sind keine Bestandteile dieses Projekts und dürfen
nicht als Implementierungsgrundlage eingeführt werden. Gleichnamige alte Projekte außerhalb
dieses Repositorys sind keine Quelle für die Unreal-Architektur.

Bestehende Unreal-C++-Klassen, Blueprints, Maps, Assets, Konfigurationen, Gameplay-,
Multiplayer-, Story-, Charakter-, Inventar-, Kampf-, Quest- und Animationssysteme erhalten.
Die englischen Begriffe `Forge` für eine Schmiede und `Fabric` für Stoffmaterialien sind
zulässige Spiel-/Materialbezeichnungen und kein Beleg für fremde Modding-Technik.

Der Editor-Target ist `DarkBloodEditor` (`Win64`, `Development`). Seine Definition liegt in
`Source/DarkBloodEditor.Target.cs`; ein eigener Ordner `Source/DarkBloodEditor/` ist dafür
nicht erforderlich. Engine-Plugins können über die `.uproject` aktiviert sein, ohne dass ein
projektlokaler `Plugins/`-Ordner existiert.

Vor destruktiven Änderungen den tatsächlichen Git-Zustand prüfen und sichern. Nur belegte
Fehländerungen entfernen. Ein erfolgreicher Build oder Projektstart ist keine Abnahme der
Grafikqualität, Bossproduktion oder aller Gameplay-/Multiplayerfunktionen.
