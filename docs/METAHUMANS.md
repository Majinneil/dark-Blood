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

## Speicherbedarf beim Bauen

Der optimierte Build braucht beim Erzeugen des gemeinsamen Ordners `Build/Common` (nach dem Kompilieren von
`Face_ControlBoard_CtrlRig`) über **110 GB zugesagten Speicher** (mit vergrößerter Auslagerungsdatei gemessen: 166 GB) – unabhängig von der Texturgröße (geprüft
2026-10-26: Akaza mit 4K/2K und NPC_King mit 2K stürzten beide bei 108–109 GB mit „Auslagerungsdatei ist zu klein“
ab). Mit 32 GB RAM und großer Auslagerungsdatei stürzt er nicht mehr ab, lagert aber so stark aus, dass der Rechner kaum noch bedienbar ist und der Build stockt (2026-10-26 abgebrochen) – praktikabel erst mit 64 GB RAM oder über Nacht. Auf einem Rechner mit 32 GB RAM muss die Auslagerungsdatei dafür vergrößert werden (z. B. 128 GB zusätzlich auf
einem Laufwerk mit Platz), sonst bricht jeder Build ab. Das Skript setzt Texturquellen trotzdem auf Spielgröße
(Akaza: Gesicht 4K, Körper 2K; NPCs 2K), weil 8K-Quellen zusätzlich Speicher und Videospeicher kosten.
Bauen am besten je Figur in einem eigenen Editor-Prozess:
`UnrealEditor.exe DarkBlood.uproject "-ExecCmds=py <Pfad>/db_metahuman_cast.py finish <Name>,QUIT_EDITOR"`.

## Unbeaufsichtigter Bau: so läuft er durch

Stand 2026-11-07 (UE 5.8.4, RTX 4070 SUPER, 32 GB RAM). So gebaut dauert Akaza etwa 2 Minuten:

```
UnrealEditor.exe DarkBlood.uproject /Engine/Maps/Entry
  -dpcvars=Editor.AsynctextureCompilationMaxConcurrency=1,r.SkinCache.Mode=0,D3D12.ResidencyManagement=0,D3D12.ResourcesStartResident=1
  "-ExecCmds=py <Pfad>/Tools/UE58/db_metahuman_build.py finish Akaza rebuild" -nosplash -unattended -nosound
```

Vorher: sechs Läufe über rund eine Stunde (Startkarte `L_Realm`, Skript direkt aus `-ExecCmds`) endeten alle beim
Fertigstellen von `SKM_Akaza_BodyMesh` mit `DXGI_ERROR_DEVICE_HUNG`. Aftermath meldete keine Zeitüberschreitung, sondern
einen Seitenfehler: Die Kopier-Einheit liest eine nicht eingeblendete Adresse (`PageFault`, `AddressTranslationError`,
`CopyEngine`), bei nur 3,6 von 11,2 GB belegtem Videospeicher.

Was geprüft wurde:
- `r.SkinCache.Mode=0` allein und `-nod3dasync -DisableAsyncCompute` änderten nichts.
- `D3D12.ResidencyManagement=0` allein stürzt nach 30 Sekunden ab, weil Ressourcen ausgelagert starten
  (`D3D12.ResourcesStartResident` gehört dazu).
- Die Kombination aus den beiden D3D12-Schaltern, leerer Startkarte und spätem Start
  (`db_metahuman_build.py`: nach 120 Bildern statt in Bild 0) läuft durch. Welcher Teil davon nötig ist und warum der Bau
  jetzt nur noch Minuten dauert, wurde nicht einzeln geprüft.

Während des Baus darf nur ein Editor laufen, und jede Figur braucht einen eigenen Prozess: Ohne Auslagerung läuft sonst der
Videospeicher voll (`NPC_Captain` stürzte mit „Out of video memory“ ab, als zweite Figur im selben Prozess und neben weiteren
Editoren).

Die Schalter gelten nur für den Bau, nicht für das Spiel. Das Spiel lief danach mit `BP_Akaza` und normalen Einstellungen
ohne Absturz (ein verdeckter Lauf von rund drei Minuten).

Python meldet jede Fehlerzeile des Baus als `RuntimeError`, auch wenn er durchläuft (`Cannot break link` der Steuerungs-Rigs,
Skelett-Abgleich des ARKit-Mappings beim Speichern). `db_metahuman_cast.py` entscheidet deshalb über das vorhandene
`BP_<Name>` und speichert danach selbst, denn der Bau lässt alles ungespeichert im Speicher. Der Blueprint liegt unter
`Build/<Name>/<Name>/BP_<Name>`. Die Haarfarbe setzt das Skript direkt in den Materialien des Builds (`HAIR_COLOUR`), weil die
Farbparameter des Creators nur in einem fertigen Vorschau-Zusammenbau vorhanden sind (`db_metahuman_cast.py colour <Name>`).

## Offen

- **Kleidung:** MetaHuman bringt nur moderne Kleidung mit; Akazas Gewand entsteht deshalb in Blender
  ([AKAZA_OUTFIT.md](AKAZA_OUTFIT.md)). NPC-Kleidung folgt nach demselben Verfahren.
- **NPC-Kleidung fehlt:** Alle neun Figuren sind gebaut (2026-11-07). König, Hauptmann und Dorfbewohner tragen aber noch
  Epics Standardkleidung (Unterwäsche) und erscheinen so im Spiel, sobald ihr Build lokal vorhanden ist.
- **Akaza:** Narben und lange Locken fehlen, Gesichtsanimation ist noch nicht geprüft.
- **Gemessen** (2026-11-07, verdeckt mit `-RenderOffscreen`, 1600×900, Figuren stehen, ohne Gefecht): 82 Bilder pro Sekunde
  mit Akaza und den Figuren der Siedlung (21 Figuren), 62–63 mit acht weiteren MetaHumans dicht vor der Kamera (29 Figuren).
  Dazu ein Durchlauf, keine Messung im echten Spielablauf.
- `DBSpawnNpc` blendet die Spielfigur aus; das trifft Akazas Gewand, aber nicht seinen MetaHuman-Körper. Er steht dann ohne
  Gewand im Bild. Das betrifft nur diesen Testbefehl.
- Ein automatisches Anpassen des Gesichts an ein Hyper3D-Modell (`db_conform_metahuman_head.py`) verzerrt das Gesicht,
  weil Bart und Haare dort feste Geometrie sind – Gesichter werden im Creator von Hand angepasst.
