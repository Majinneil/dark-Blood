# MetaHumans: Held und menschliche NPCs

Die menschlichen Figuren von DARK BLOOD entstehen mit dem **MetaHuman Creator** in Unreal Engine 5.8: echte Haut,
Strähnenhaar, Gesichts-Rig. Bosse und Dämonen bleiben bei ihren eigenen Körpern (Paragon, später eigene Modelle).

## Voraussetzungen (einmalig)

1. Epic Launcher → UE 5.8 → *Optionen* → **MetaHuman Creator-Kerndaten** (5,75 GB) installieren.
2. Plugin **MetaHumanCharacter** ist in `DarkBlood.uproject` eingeschaltet; die Projekt-Einstellungen
   `r.GPUSkin.Support16BitBoneIndex` / `r.GPUSkin.UnlimitedBoneInfluences` stehen in `Config/DefaultEngine.ini`.
3. **Epic-Freigabe im Editor:** Auto-Rigging und Texturen laufen über Epics Cloud. Beim ersten „Rig erstellen“
   öffnet der Editor eine Epic-Anmeldeseite im Browser – dort bestätigen. Ohne Rig lässt sich kein MetaHuman bauen.

## Die Besetzung

| Asset (`/Game/DarkBlood/Characters/MetaHumans/`) | Rolle | Vorlage | Profil im Spiel |
|---|---|---|---|
| `Akaza` | Held Akaza Kurosaki (Spieler, Körpertyp A) | Bruce, Haare „Short Pulled Back“, Vollbart | `CV_MH_Akaza` |
| `NPC_King` | König | Walter | `CV_MH_NPC_King` |
| `NPC_Captain` | Hauptmann | Kelvin | `CV_MH_NPC_Captain` |
| `Villager_01` … `_06` | Dorfbewohner (Wahl nach Name) | Bo, Aera, Aoi, Sook-ja, Tuya, Mateo | `CV_MH_Villager_0N` |

Die Assets sind lokal (MetaHuman-Ausgabe, ignoriert) und werden mit den Skripten neu erzeugt:

- `Tools/UE58/db_create_akaza_metahuman.py [Vorlage] [fresh]` – Akaza aus einer Vorlage.
- `Tools/UE58/db_metahuman_cast.py create` – NPC-Besetzung aus Epic-Vorlagen.
- `Tools/UE58/db_metahuman_cast.py finish [Name]` – Cloud-Rig (nur Gelenke, günstig zur Laufzeit) und optimierter
  Zusammenbau (Qualität *Medium*) jeder Figur nach `.../MetaHumans/Build/<Name>/BP_<Name>`.

Alle Befehle im Editor-Konsolenfeld mit `py "<Pfad>" <Argumente>` – **nicht** mit `-ExecutePythonScript` auf der
Kommandozeile (UE 5.8 schließt den Editor danach) und nie aus einem offenen MetaHuman-Editor heraus dessen Asset
löschen (Absturz).

## Im Spiel

`DBDevelopmentContent` registriert jeden Build `BP_<Name>` als Profil `CV_MH_<Name>`: Der unsichtbare UE5-Mannequin
spielt alle Animationen (Animationsset `AS_Dev_Mannequin`), der MetaHuman wird als **VisualActorClass** angehängt, sein
Körper folgt dem Mannequin über die Knochennamen (Leader Pose), das Gesicht kopiert die Körperpose selbst. Ohne Build
gilt der bisherige Körper – nichts bricht.

- Spieler (Körpertyp A): `CV_MH_Akaza`, sonst `CV_Hyper3D_Akaza`, sonst Mannequin.
- NPCs: `CV_MH_<NpcId>` (Dorfbewohner: `CV_MH_Villager_0N` nach Namen), sonst `CV_<NpcId>`, sonst `CV_NPC_Default`.

Geprüft mit einem Stellvertreter-Blueprint (Quinn als `BP_NPC_Captain`): Der Hauptmann erschien als Quinn in der
Idle-Pose des unsichtbaren Mannequins, 65,8 FPS. `DBSpawnNpc <NpcId> [Name]` stellt eine Figur vor die Kamera.

## Offen

- **Kleidung:** MetaHuman bringt nur moderne Kleidung mit. Kostenlos und ohne KI-Sperre gibt es auf Fab keine
  japanische MetaHuman-Kleidung; „EDO_CITIZEN_X“ (DarkMattersForge, gratis, Edo-Zeit) ist mit „Allows usage with AI:
  No“ markiert – Entscheidung des Projektinhabers.
- Erst geriggte und gebaute MetaHumans (nach der Epic-Freigabe) zeigen das echte Ergebnis; danach FPS-Messung mit
  mehreren MetaHumans gleichzeitig.
- Ein automatisches Anpassen des Gesichts an ein Hyper3D-Modell (`db_conform_metahuman_head.py`) verzerrt das Gesicht,
  weil Bart und Haare dort feste Geometrie sind – Gesichter werden im Creator von Hand angepasst.
