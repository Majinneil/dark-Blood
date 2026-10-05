# Roadmap und Status

Legende: ✅ fertig und getestet · 🟡 implementiert, nicht (vollständig) getestet · ⬜ offen

| Phase | Inhalt | Status |
|---|---|---|
| 0 | Preproduction: Analyse, Architektur, Struktur, Konventionen, Dokumentation | ✅ |
| 1 | Technisches Fundament | 🟡 Regelkern ✅ · UE-Schicht kompiliert (UE 5.8.3, Win64), **nicht laufzeitgetestet** |
| 2 | Character & Combat Vertical Slice | ✅ Gameplay (Platzhalter, headless + Koop getestet) · 🟡 Animationen/VFX/Audio (Assets fehlen) |
| 3 | Erster Story Vertical Slice | ✅ Gameplay (Dialoge, Quests, Charaktererstellung, Slate-UI; headless + Koop getestet) · 🟡 Präsentation (Assets) |
| 4 | Klassen & Skilltrees | ✅ Gameplay (4 Kits, 8 Signaturfähigkeiten, Skilltrees, UI; headless getestet) · 🟡 Animationen/VFX |
| 5 | Inventar / Loot / Crafting | ✅ Gameplay (Werte, Resistenzen, Verbrauch, Beute, Crafting, Reparatur, UI; headless getestet) · 🟡 Icons/Meshes |
| 5.5 | Visual Foundation (Codex-Paket) | ✅ Systeme, Materialsystem, Baukasten, Licht, Visual Slice (gerendert + Koop getestet) · 🟡 echte Assets (Texturen, Bäume, MetaHumans, Animationen) |
| 6 | Open World | ✅ 16-km-Welt, 16 Regionen, 17 Siedlungen, Meer, Schiffe, Wälder/Gras, Weltkarte `[M]`, Schwimmen, Regionsstimmung (gerendert + Koop getestet) · 🟡 Klippen/Wasserfälle/Inselküsten → Phase 11/17 |
| 7 | NPC- und Siedlungssimulation | ✅ Gameplay (17 Siedlungen abstrakt simuliert + gespeichert, Dorfbewohner, Angriffe als Kämpfe; Regelkern + headless getestet) · 🟡 Mass-Mengen, Tagesabläufe, Händler, Animationen |
| 8 | Reise / Pferde / Kutschen / Survival | ✅ Gameplay (Pferd, Kutschen-Schnellreise, Sättigung/Wärme, Schwimm-Ausdauer; Regelkern, headless, Koop getestet) · 🟡 Modelle/Animationen |
| 9 | Dungeon-System | ✅ Gameplay (8 Dungeons, Generator aus Seeds, Kämpfe/Fallen/Schatz/Schrein/Wächter, Fortschritt gespeichert; Regelkern, headless, Koop, gerendert getestet) · 🟡 eigene Themen-Modelle, Rätsel |
| 10 | Boss-Framework | ✅ Gameplay (16 Vasallen + Dämonenkönig mit 3 Formen + Dungeon-Wächter, Arenen, Spezialangriffe, Phasen, Raserei, Koop-Skalierung, Boss-Leiste, Kartenmarker; Regelkern, headless, Koop, gerendert getestet) · 🟡 Modelle, Animationen, Musik, Intros |
| 11 | 14 Regionen | ✅ Gameplay (Dämonenrudel je Gebiet nach Zustand und Tageszeit, 14 Dämonenlager mit Hauptmann, Befreiungsquests, Kartenmarker; Regelkern, headless, Koop, gerendert getestet, 76–92 FPS) · 🟡 eigene Gegnermodelle, Landmarken, Musik |
| 12 | 16 Vasallen | ✅ Gameplay (16 eigene Signatur-Attacken, Kampfsprüche, Diener in Bossstärke; Regelkern, headless getestet) · 🟡 Heldenmodelle, Animationen, Musik je Vasall |
| 13 | DAS ENDE | ✅ Gameplay (Tor des Endes mit Blutsiegel, Questkette MQ10–MQ12, Letzte Bastion als Ruhepunkt, Pfad der Schande; headless und gerendert getestet, 98 FPS) · 🟡 schwebende Inseln, Festungsarchitektur |
| 14 | Dämonenkönig | ✅ Gameplay (drei Formen mit eigener Katastrophe, Sprüche je Form, Arena-Verwandlung, weltweite Meldung, Hauptquest MQ12; headless und gerendert getestet, 90–106 FPS) · 🟡 Modell mit Flügeln, Animationen, Musik, Zwischensequenzen |
| 15 | Paradies & Finale | ✅ Gameplay (Welt gereinigt nach dem König, Pforte aus Licht am Thron, schwebende Paradies-Insel über DAS ENDE, Schrein des Friedens mit Finale und Abspann, Quest MQ13, Rückkehr in die Hauptstadt; Regelkern, headless, Koop, gerendert getestet, 92–100 FPS) · 🟡 Zwischensequenz, Musik, Ahnengeister |
| 16 | Endgame | ⬜ |
| 17 | High-End Visual Overhaul | ⬜ |
| 18 | Audio / Voice / Cinematics | ⬜ |
| 19 | Multiplayer Hardening | ⬜ |
| 20 | Optimierung | ⬜ |
| 21 | QA & Release | ⬜ |

---

## Bekannter Blocker

**In der Entwicklungsumgebung (Cloud-Container) ist keine Unreal Engine verfügbar.** Der Engine-Quellcode ist
nur über ein mit Epic verknüpftes GitHub-Konto erreichbar, und ein Engine-Build übersteigt den verfügbaren
Speicher. Folgen:

- Der UE-C++-Code (`Source/DarkBlood`) wurde gegen die UE-5.5/5.6-API geschrieben, aber **nie kompiliert**. Mit UE 5.8 sind zusätzlich
  Deprecation-Warnungen oder geänderte Signaturen möglich.
  Beim ersten Build sind Kompilierfehler wahrscheinlich und müssen behoben werden.
- Keine Laufzeit-, PIE- oder Multiplayer-Tests der UE-Schicht.
- Assets (`.uasset`, Karten) können hier nicht erstellt werden. Deshalb erzeugt der Code klar markierte
  Entwicklungsdaten und eine Entwicklungs-Steuerung.

**Auflösung:** Projekt lokal mit UE 5.8 bauen (oder Claude Code lokal mit installierter Engine nutzen), Fehler
beheben und danach die Tests aus Phase 1 (unten, „Manuelle Tests“) durchführen. Optional: GitHub-Actions-Runner
auf einem Rechner mit Engine für automatische UE-Builds.

---

## Abschlussbericht Phase 0 – Preproduction

**ERSTELLT**
- Projektstruktur, `DarkBlood.uproject`, Targets (Game/Editor/Server), Config, `.gitattributes` (Git LFS),
  `.gitignore`, `.editorconfig`, `.clang-format`
- Dokumentation: README und 15 Dokumente unter `docs/`
- Architekturentscheidungen: Regelkern-Modul, Persistenz auf PlayerState, serverautoritatives Modell mit
  zwei Persistenzmodi, datengetriebene Assets, Trennung Gameplay/Visual

**ANALYSE:** Repository war leer (nur README). Keine bestehenden Systeme zu erhalten.
**UE-VERSION:** Zuordnung 5.8 (vom Nutzer installierte Launcher-Version, zuvor 5.6). Der Code nutzt APIs ab 5.5 (z. B. `FGameplayAbilitySpec::GetDynamicSpecSourceTags`,
`AActor::SetNetUpdateFrequency`).
**PLUGINS:** GameplayAbilities, EnhancedInput, MotionWarping, StateTree, GameplayStateTree, ModelingTools
(Editor), Python + EditorScriptingUtilities (Editor). Niagara, Control Rig und World Partition sind
Engine-Standard.

## Abschlussbericht Phase 1 – Technisches Fundament

**ERSTELLT**
- `Source/DarkBloodRules`: Name, UTF-8, Progression, Stats/Stärke/Gefahrenstufe, Schaden, Items/Katalog,
  Inventar mit Taschen, Equipment, Todesstrafe, Skilltree-Regeln, Quests, Weltstatus (Uhr, Regionen, Vasallen,
  DAS ENDE), Binärarchiv mit CRC32, Charakter-/Welt-Datensätze, Upload-Validierung
- `Tests/RulesTests`: eigener Mini-Testrahmen, 30 Tests, CMake mit `-Werror`, ASan/UBSan-Option
- `Source/DarkBlood`: Datenassets (Klasse/Item/Quest/Region), Daten-Subsystem mit Entwicklungsdaten,
  GAS (AttributeSet, ASC mit Input-Tags, Basis-Ability, AbilitySet, Schadens-Execution, Regeneration),
  Enhanced Input (Config-Asset + Code-Fallback), Spielercharakter, PlayerController, PlayerState,
  Progression-, Inventar-, Quest- und Weltstatus-Komponente, Quest-Subsystem, Regionsvolumen, GameMode,
  GameState, Save-Subsystem, CheatManager, Debug-HUD
- CI: `.github/workflows/rules-tests.yml` (Linux + Windows, Debug + Release)

**GEÄNDERT:** `README.md`

**FUNKTIONIERT (getestet, Regelkern)**
- Freie Charakternamen inkl. Umlaute/ß, Kana/Kanji; Normalisierung; Rufname („Jin Akagi“ → „Jin“)
- XP-Kurve 1–100, Mehrfach-Levelaufstiege, Skillpunkte alle 5 Level, Deckelung, Manipulationserkennung
- Abgeleitete Werte, Stärke, Gefahrenstufen (z. B. Stärke 13 bei 50–60 → EXTREM, Betreten nie verhindert)
- Schaden: Rüstung, Resistenzen (gedeckelt), Krit, Block mit Ausdauerkosten, Perfect Parry, i-Frames
- Inventar: Stapeln, Alles-oder-nichts, Einzelinstanzen, Verschieben/Teilen/Tauschen, Questitems ohne Slots
- Taschen: +9/+18/+27, Spezialtaschen mit Kategoriefilter; zu kleine Tasche wird **abgelehnt**, ohne dass
  ein Item verloren geht (Zählprobe vor/nach)
- Equipment mit Slot-, Level- und Klassenprüfung; volles Inventar blockiert Ablegen statt Item zu vernichten
- Tod: Inventar unverändert, 5 % Mon-Verlust (max. 500), Haltbarkeit −5 %
- Skilltree: Kosten, Ränge, Level, Voraussetzungen
- Quests: sequenziell/parallel, optionale Ziele, Story-Flags, Abgabe, Hauptquest nicht abbrechbar
- Welt: Zwischenboss → umkämpft, Vasall → befreit (idempotent), DAS ENDE nach den 14 Vasallen draußen (16 insgesamt, 2 bewachen DAS ENDE),
  zeitschrittunabhängige Erholung, Tag/Nacht
- Speichern: Round-Trip byte-identisch, Prüfsumme, Versionsprüfung, Kürzungs-/Fuzz-Robustheit,
  Ablehnung manipulierter Uploads (duplizierte Instanzen, negative Währung, falsches Level, ungültiger Name,
  unverdiente Skillpunkte)

**TEILWEISE (geschrieben, nicht kompiliert/getestet)**
- Gesamte UE-Schicht (siehe oben)

**OFFEN (bewusst späteren Phasen zugeordnet)**
- Kampfaktionen (Angriffe, Dodge, Lock-On …), Trainingsgegner → Phase 2
- Charaktererstellungs-UI, echte UI (UMG/CommonUI) → Phase 3
- Klassenfähigkeiten und Skilltree-Inhalte → Phase 4
- Ausrüstungs-Stat-Boni, Elementarresistenzen als Attribute → Phase 5
- Echte Hauptwelt `L_Realm` (World Partition) → Phase 6
- Party-System: aktuell zählen Kill-/Custom-Ereignisse für alle Spieler der Sitzung → Phase 3/19

**TESTS:** `ctest` – 30/30 bestanden (GCC 13, Debug, ASan+UBSan; Clang 18, Release). Zusätzlich
`-Wconversion`-Prüfung mit Clang ohne Befund.
**BUILD:** Regelkern ✅. UE-Module: `DarkBloodEditor Win64 Development` mit UE 5.8.3 / VS 2022 fehlerfrei und
ohne Warnungen (Unity und `-DisableUnity`). Regelkern-Tests zusätzlich mit MSVC bestanden.
**LAUFZEIT (headless, `-game -nullrhi`, Karte `/Engine/Maps/Entry`):** Tests 2–6 unten per `-ExecCmds` bestanden:
Levelaufstieg 1 → 6, Tasche ausgerüstet, MQ01 abgeschlossen (Belohnung erhalten), Tod mit 5 % Mon-Verlust und −5 %
Haltbarkeit der ausgerüsteten Waffe, Respawn nach 5 s, Zustand nach Neustart identisch. Dabei behoben:
`DBDamageSelf` nutzte `ApplyModToAttribute` (umgeht `PostGameplayEffectExecute`, kein Tod) und wendet jetzt einen
Instant-Effekt an. Test 1 (Debug-Overlay) braucht Rendering und ist offen.
**MULTIPLAYER:** Replikationsdesign implementiert (siehe MULTIPLAYER.md). Listen-Server + Client (headless):
Client verbindet, lädt Charakter „Jin Akagi“ hoch, Server nimmt ihn an. Namensschilder (Rendering) nicht geprüft.
Dev-Kommandos sind im Listen-Server außerhalb des Editors gesperrt (`AllowCheats`).
**PERFORMANCE:** keine Messungen möglich. Designentscheidungen: Fast-Array-Delta für Inventar, quantisierte
Regionswerte, Uhr-Extrapolation statt Tick-Replikation.

### Manuelle Tests nach dem ersten UE-Build

1. Editor öffnen, Play (Standalone): Debug-Overlay zeigt „Wanderer“, Krieger, Stufe 1, Leben/Ausdauer/Mana.
2. `DBGiveXp 5000` → Levelaufstieg, Maximalwerte steigen, Leben aufgefüllt.
3. `DBGiveItem Bag_Adventurer 1`, `DBDumpCharacter`, `DBEquipBag <Section> <Index>` → Kapazität +18.
4. `DBDamageSelf 99999` → Tod, 5 s später Respawn, Items unverändert (`DBDumpCharacter`).
5. `DBQuestEvent Talk NPC_King 1`, dann 3× `DBQuestEvent Kill TrainingDummy 1` → Belohnung, Story-Flag.
6. `DBSaveAll`, PIE beenden, neu starten → Zustand identisch.
7. PIE mit 2 Spielern (Listen Server): Client lädt eigenen Charakter hoch, Namensschilder zeigen Charakternamen.

## Zwischenstand Phase 2 – Character & Combat Vertical Slice

**ERSTELLT**
- Regelkern `Combat.h`: Combo-Fenster, Aufladen, Trefferbogen, Ausdauerregeln, Poise → Flinch/Stagger/Knockdown,
  Lock-On-Bewertung; 6 neue Tests (36/36 bestanden, MSVC)
- GAS: Light-Combo, Heavy/Charged, Block/Perfect Parry/Konter, Dodge mit i-Frames, Sprint, Trefferreaktion;
  Schadens- und Ausdauer-Effekte; Poise-Attribute; Regenerationspausen per Tag
- `UDBLockOnComponent`, `ADBEnemyCharacter`, `ADBTrainingDummy`; Teams; Kill-XP + Quest-Kill-Ereignis
- Dev-Kommandos für skriptbare Kampftests (siehe COMBAT_SYSTEM.md)

**GETESTET (headless, `Template_Default`)**: 3er-Combo trifft und tötet die Puppe (+20 XP, Respawn), voll
geladener Heavy (112 Schaden), Puppenschlag trifft (15), Block (4,5 statt 15, Poise 6), Perfect Parry (0 Schaden,
Puppe `ParriedStagger`, Konter ausgelöst), Dodge weicht aus, Sprint, Lock-On. Gerendert: Screenshots mit Debug-HUD,
Namensschild/HP der Puppe.

**KOOP (Listen-Server + Client, headless)**: Client-Combo, Knockdown, Perfect Parry des Clients, Konter (×2),
geladener Heavy, Dodge – serverseitig bestätigt. Dabei behoben: Client-Absturz (GAS-Assertion), wenn ein Gegner
repliziert wurde, bevor sein BeginPlay lief.
**GEGNER**: Niederer Dämon mit einfacher Nahkampf-KI – verfolgt, greift an, wird geblockt/gestaggert, stirbt, 45 XP.

**KONTEXT-ANGRIFFE & DOPPELSPRUNG**: Doppelsprung (145 → 264 cm), Luftangriff (Knockdown), Sprint-Angriff, Dash-Angriff –
einzeln und im Koop getestet. Dabei behoben: passiver Doppelsprung wurde für Remote-Clients nie aktiviert
(jetzt serverseitig + repliziertes Tag).

**OFFEN (Assets / spätere Phasen)**: Animationen, VFX/Audio (GameplayCue-Notifies), KI mit Navigation/StateTree,
Balancing mit Klassen-Kits.

## Abschlussbericht Phase 3 – Erster Story Vertical Slice

**ERSTELLT**
- Regelkern `Dialogue.h`: Dialoggraph mit Einstiegsknoten nach Story-Flags/Queststatus, gefilterten Optionen,
  Effekten (Story-Flag, Quest starten/abgeben, Talk-Ereignis) und Validierung; der Server lehnt nicht angebotene
  Optionen ab. 3 neue Tests (39/39 bestanden)
- `UDBDialogueDefinition` (Asset-Typ `DBDialogue`), `UDBDialogueComponent` (serverseitige Gesprächsführung am
  PlayerController), `ADBNpcCharacter`, Interaktionssystem (`IDBInteractable`, `UDBInteractionComponent`, `[E]`)
- `ADBEncounterSpawner` (Gegnergruppen nach Quest/Story-Flag)
- Oberfläche in Slate (ohne Widget-Assets): HUD (Leben/Ausdauer/Mana, Quest-Tracker, Lock-On-Ziel mit Leben/Poise,
  Interaktionshinweis, Meldungen), Dialogfenster (Optionen per Klick oder 1–4), Charaktererstellung (Name, Klasse,
  Körpertyp) – `ADBGameHUD`; Debug-Overlay jetzt standardmäßig aus
- Charaktererstellung ersetzt den Entwicklungscharakter, wenn gerendert wird und kein Charakter existiert
  (Host, Client und ServerAuthoritative); Skripte/Headless nutzen weiter `-DBCharacterName`
- Story-Slice (DEV): König Aoki → MQ01 „Der Ruf des Königs“ (3 Übungspuppen, Abgabe beim König) →
  MQ02 „Schatten vor dem Osttor“ (Hauptmann Kenji, 3 niedere Dämonen, Abgabe beim Hauptmann);
  `-DBDevSlice` / `DBSetupSlice` baut ihn in jeder Karte auf
- Farbige Platzhalter (Spieler blau, Gegner rot, NPCs gold, Puppen holzfarben)
- Dev-Kommandos: `DBSetupSlice`, `DBGoto <Id>`, `DBDialogueChoose <n>`, `DBCreateCharacter <Klasse> <Name>`

**GETESTET**: kompletter Slice headless (Dialogpfade, Quest-Fortschritt, Abgaben, Encounter, Belohnungen,
Meldungen) und im Koop (Client führt Dialog und Quests, Server wendet an); gerenderte Screenshots von
Charaktererstellung, Dialog und HUD.

**OFFEN**: Chronik/Questbuch-Fenster, Inventar-/Ausrüstungs-UI (Phase 5), Gesichter/Haare im Editor (Assets),
Sprachausgabe/Cinematics (Phase 18), Party-System (Phase 19).

## Abschlussbericht Phase 4 – Klassen & Skilltrees

**ERSTELLT**: Klassen-Kits für Krieger, Schattenläufer, Magier, Mönch (siehe CLASS_SYSTEM.md) mit je zwei
Signaturfähigkeiten und 5 Skilltree-Knoten (inkl. Doppelsprung); Mana-Kosten, Abklingzeiten, Projektile,
Schutzkreis-Aktor, Flug; Skilltree-Fenster `[K]` und Fähigkeitenleiste mit Abklingzeiten im HUD; Regelkern:
`DamageTakenMultiplier`/`BlockStaminaMultiplier` (+1 Test, 40/40); `-DBCharacterClass`, `DBUnlockSkill`.

**GETESTET (headless)**: je Klasse Freischaltung nach Regeln (Maximalrang wird abgelehnt), alle Signaturfähigkeiten
und passiven Knoten mit messbarer Wirkung (z. B. Haltung 50→90 Poise und 18,9→14,9 Schaden, Kettenblitz 22,6→13,6→8,1,
Konterhaltung 47,1 Konter + Knockdown, Rauchschleier: KI verliert das Ziel); gerenderte Screenshots von Skilltree und
Fähigkeitenleiste.

**BEKANNT**: Die einfache Nahkampf-KI steuert direkt und bleibt an Hindernissen hängen → Navigation mit der
Hauptwelt (Phase 6).

## Abschlussbericht Phase 5.5 – Visual Foundation

**ERSTELLT**: Charakter-Profile + Visual-Komponente (Slots, Qualitätsstufen, nur Ids repliziert), Aussehen in der
Charaktererstellung, Animations-Sets nach `Anim.*` und AnimInstance-Basis, Materialsystem (12 Master, 40 Instanzen,
per Skript erzeugt), modularer Gebäude-Baukasten mit Kit-Schnittstelle, Tore/Laternen/Wege/Mauern/Bach/Brücke/
Dungeon-Eingang, regelbasierte Vegetation, Licht-Presets, Visual Slice um den Story-Slice, Setup-/Audit-Werkzeuge.
Details: VISUAL_FOUNDATION.md, Stand/Blocker/nächste Schritte: VISUAL_FOUNDATION_STATUS.md.

**GETESTET**: Build (Unity + Non-Unity), Regelkern 44/44, Headless-Regression (Kampf, Klassen-Kit, Crafting),
Koop mit identischem Slice auf Server und Client, gerenderte Ansichten aller Bereiche bei Tag/Dämmerung/Nacht/
Dämonennacht, ≈ 5 ms GPU.

**OFFEN** (Assets, vom Nutzer zu beschaffen): Poly-Haven-Texturen, Fab-Free-Assets, CC0-Vegetation, Game Animation
Sample, MetaHumans; NO-AI-Assets nur manuell.

## Abschlussbericht Phase 5 – Inventar, Loot, Crafting

**ERSTELLT**: Regelkern `Crafting.h` + `FItemStats`/`FConsumableEffect` (4 neue Tests, 44/44); Resistenz-Attribute;
Ausrüstungswerte in den Charakterwerten; Benutzen/Craften/Reparieren im Inventar; Datenassets `DBRecipe`/`DBLootTable`;
Schmiede und Truhe; persönliche Gegner-Beute; Inventar-UI `[I]` und Schmiede-Fenster; neue Charaktere tragen ihre
Startwaffe. Details: ITEMS_AND_CRAFTING.md.

**GETESTET (headless)**: Waffe anlegen (AP 60→64), Truhe (Helm, Tränke, Material, Mon; einmal pro Charakter), Helm
(+8 Rüstung, +15 Leben), Tamahagane-Katana schmieden (Zutaten und 80 Mon abgezogen, zweiter Versuch abgelehnt,
AP →74), Heiltrank (+120), Tod → Haltbarkeit −5 %, Reparatur 7 Mon, Dämonenbeute. Gerendert: Inventar und Schmiede.

## Zwischenstand Phase 6 – Offene Welt, Siedlungen, Schiffe (Stand 2026-10-03)

**ERSTELLT**: Offene Welt `L_Realm` (16 × 16 km, 16 Regionen nach der Weltkarte, 17 Siedlungen inkl. Hauptstadthafen,
Wald/Felsen in 1-km-Zellen, Meer mit Wellen-Material); Siedlungen mit geplanten Straßen, Blockfüllung und über Frames
gebauten Instanz-Blöcken; vier segelbare Schiffstypen (Kriegs-, Kampf-, Handelsschiff, Boot) mit freien Dschunken-
Modellen und gemessener Deckhöhe; 18 freie Sketchfab-Modelle (CC BY, siehe CREDITS.md) für Häuser, Burgen, Pagoden,
Tempel, Torii; ländliche Siedlungen auf Wiese mit Dorfplatz und Höfen statt Erdscheibe. Details: WORLD_DESIGN.md,
Leistung: PERFORMANCE.md.

**GETESTET**: Build; gepackter Development-Build (Großstadt 105 FPS Hoch / 164 FPS Niedrig bei 1600×900); Segeln im
Einzelspieler und Koop (Kit-Schiff); Stehen auf allen vier Modell-Decks und Segeln mit dem Boot; Kampf-Regression
unverändert.

**OFFEN**: 160 FPS auf „Hoch“ nur mit Frame Generation (FSR 3 oder DLSS – Entscheidung des Nutzers);
Schiffs-Innenräume und Besatzung; Koop-Test mit den Modell-Schiffen; Vasallen-Konzeptblätter unter `References/` sind
noch nicht umgesetzt.

## Abschluss Phase 6 – Open World (Stand 2026-10-05)

**ERGÄNZT**: Startkarten (Spiel, Editor, Server) auf `L_Realm`; Weltkarte `[M]` (gemalte Karte, Spielerpfeil, Mitspieler,
Siedlungen, aktuelles Gebiet; Projektion `DBRealm::ToMapPixel`); Schwimmen (`UDBCharacterMovementComponent`, alle
Charaktere, Wasser = alles unter Meereshöhe); Regionsstimmung (`UDBRealmMoodComponent`: Nebel, Sonne, Farbkorrektur je
Gebiet, weiche Übergänge); Testbefehl `DBWalk`. Behoben: Umgebungspartikel als 1-m-Kugeln, Koop-Speicherfehler beim
Beenden mit verbundenen Gästen.

**GETESTET**: Build ohne Warnungen; Regelkern 44/44; Karte gerendert (Kirschblütental, Hauptstadt); Schwimmen an der
Wüstenküste ins Meer und an Land, Einzelspieler + Koop; Stimmungen in Feuergebirge, Dämonenöde, Bambuswäldern, Dem Ende,
Eisöde gerendert, 80–85 FPS; Kampf-Regression unverändert.

**VERSCHOBEN** (braucht Neuaufbau des Geländes bzw. Assets): Klippen, Wasserfälle, zerklüftete Inselküsten und
schwebende Inseln (Himmelstempel, Das Ende) der Kartenvorlage → Phase 11 (Regionsinhalte) / 17 (Visual Overhaul);
Gras auf gepflasterten Siedlungsplätzen; Schwimmanimation; Tauchen.

## Abschluss Phase 10 – Boss-Framework (Stand 2026-10-05)

**ERGÄNZT**: 16 Vasallen statt 14 (13 Gebiete + Vasallenfestung; Tsukigami und Shirogane bewachen DAS ENDE), Weltstand
v4 mit Zählung je Vasall und Migration alter Stände; Dämonenkönig mit drei Formen; Boss-Regeln im Regelkern
(Koop-Skalierung, Phasen, Raserei); `UDBBossDefinition`, `ADBBossCharacter` (Stampfer, Sturmangriff, Salve,
Gefahrenzone, Beschwörung), `ADBBossTelegraph`, `ADBBossArena` (Steinboden, Blutbarriere, Versiegelung, Zurücksetzen,
flache Platzierung, räumt Bäume weg); Dungeon-Wächter als Boss; Boss-Leiste im HUD; Bossmarker auf der Weltkarte;
Testbefehle `DBBossList`, `DBBossArena`, `DBBossSpawn`, `DBBossDump`, `DBBossHurt`, `DBBossDefeat`.
Details: [BOSS_FRAMEWORK.md](BOSS_FRAMEWORK.md).

**GETESTET**: Build ohne Warnungen; Regelkern 61/61 (inkl. 16 Vasallen, Boss-Regeln); headless: Arena öffnet,
Phasenwechsel, Spezialangriffe treffen, Sieg befreit das Gebiet, DAS ENDE versiegelt bis 14/16, Thron versiegelt bis
16/16, König mit drei Formen, Weltstand gespeichert und wieder geladen; Dungeon-Wächter im Bossraum; Koop
(Listen-Server + Client): Kampf mit 2 Spielern, Leben ×1,45, 3 Dämonen statt 2, geteilte Aufmerksamkeit, Arenen,
Boss und Phasen auf dem Client repliziert; gerendert: Arena, Boss-Leiste, Gefahrenzone, Thron-Arena, Kartenmarker.

**OFFEN**: Heldenmodelle und Animationen nach den Entwürfen, Telegraph-VFX, Musik, Intros/Finisher, eigene
Abilities/StateTree je Boss (Phase 12/14), Arena-Transformation des Königs.

## Abschluss Phase 11 – Regionsinhalte (Stand 2026-10-05)

**ERGÄNZT**: Gebietsdämonen (`UDBRegionLifeComponent`): Rudel um jeden Spieler, Anzahl nach Gebietszustand und
Tageszeit, Stufe und Werte aus dem Stufenband des Gebiets, Elite-Anführer; 14 Dämonenlager (`ADBDemonCamp`) mit
Hauptmann (Zwischenboss, 14 neue Bossdefinitionen) und Wachen, das Lager macht das Gebiet umkämpft; Befreiungsquests
`RQ_Region01`–`RQ_Region14` starten beim Betreten; Lagermarker auf der Karte; Arenen mit echten Modellen (Laternen,
Tempelstatuen, Torii) und leuchtender Blutbarriere; Boss-Porträts in der Boss-Leiste. Details:
[REGIONS.md](REGIONS.md).

**GETESTET**: Regelkern 64/64 (neu: Rudelbudget, Stufen, Werte); headless: Quest startet beim Betreten, Rudel auf
Gebietsstufe, Fortschritt 6/1/1, Lager in drei Gebieten (Kirschblütental, Feuergebirge, Küstenland) im richtigen Gebiet,
Hauptmann fällt → umkämpft (Einfluss 0,7, 1 Rudel), Vasall → befreit (0 Rudel am Tag); Koop: Quest, Hauptmann, Beute
und Namen auf dem Client, gemeinsames Rudelbudget; gerendert: Lager, Questtracker, Arena. FPS 76–92 (mit freiem
Grafikspeicher; ein im Hintergrund geladenes 7,4-GB-Ollama-Modell drückte sie auf 18).

## Abschluss Phase 12 – 16 Vasallen (Stand 2026-10-05)

**ERGÄNZT**: Jeder der 16 Vasallen hat eine eigene Signatur-Attacke (Blutpfad, Eisringe, Schattenschritt, Blitzregen,
Flammenwall, Knochenarmee, Seuchenwolke, Trugbilder, Rudelruf, Eisenhaut, Flutwelle, Sturmklingen, Klingen der Nacht,
Leerensog, Blutmond, Banner des Königs) mit eigenem Rhythmus (`GetSignatureCooldown`) und Einsatz nach dem
Phasenwechsel; Kampfsprüche zu Kampfbeginn und Phasenwechsel; beschworene Dämonen in Bossstärke, die für die
Gebietsquest zählen; Testbefehl `DBBossSignature`. Details: [BOSS_FRAMEWORK.md](BOSS_FRAMEWORK.md).

**GETESTET**: Regelkern 65/65; headless: alle 16 Signaturen nacheinander ohne Fehler; Diener-Anzahl stimmt (4/2/3/2),
Treffer am stehenden Spieler bei Blutpfad, Blitzregen, Flutwelle, Sturmklingen, Leerensog, Blutmond, Klingen der
Nacht; Eisringe und Flammenwall lassen Lücken zum Ausweichen.

## Abschluss Phase 13 – DAS ENDE (Stand 2026-10-05)

**ERGÄNZT**: Tor des Endes (`ADBEndGate`) mit Blutsiegel bis 14/14, Meldung beim Brechen; Hauptquestkette
`MQ10_TheEndSeal` → `MQ11_ThroneGuardians` → `MQ12_DemonKing`, vom Tor nacheinander gestartet; Letzte Bastion
(`ADBBastionShrine`) heilt und wird Ruhepunkt; Pfad der Schande; Kartenmarker; „Erreichen“-Ziele für Gebiete; Quests
rechnen beim Start bereits besiegte Bosse an. Behoben: Wiederbelebung am Ruhepunkt (zwischengespeicherter Startpunkt
ging vor). Details: [THE_END.md](THE_END.md).

**GETESTET**: headless: Tor versiegelt → 14 Vasallen → Siegel bricht → MQ10 startet → Bastion (Ruhepunkt) und
DAS ENDE betreten → MQ10 fertig → MQ11 startet, nachträglich angerechnetes Tsukigami + Shirogane → MQ11 fertig →
MQ12 startet; Tod im Kirschblütental → Wiederbelebung in der Bastion; gerendert: Tor mit und ohne Siegel, Bastion,
Questtracker, 97–98 FPS.

## Abschluss Phase 14 – Dämonenkönig (Stand 2026-10-05)

**ERGÄNZT**: Katastrophen je Form (Kaiserliches Urteil, Blutflut, Weltenbrand mit dauerhaftem Feuerring und
Sternenfall), Sprüche zu Kampfbeginn und je Form, Verwandlung der Thron-Arena in Form 3 (Boden aus glühendem Blut,
lodernde Barriere), weltweite Meldung bei seinem Fall, Warnzonen eines Bosses verschwinden mit ihm; der Testbefehl
`DBBossDefeat` zählt auch für Quests. Details: [BOSS_FRAMEWORK.md](BOSS_FRAMEWORK.md).

**GETESTET**: headless: Kampf durch alle drei Formen mit ihren Katastrophen und Sprüchen, Blutgeburten, Sturz;
Hauptquestkette MQ10 → MQ11 → MQ12 vollständig abgeschlossen; gerendert: Form 1 und Form 3 mit verwandelter Arena,
90–106 FPS.

## Abschluss Phase 15 – Paradies & Finale (Stand 2026-10-05)

**ERGÄNZT**: Weltreinigung beim Fall des Königs (`FWorldState::PurifyWorld`: alle Gebiete und DAS ENDE befreit, kein
Dämoneneinfluss, keine Rudel mehr); Pforte aus Licht am Thron (`ADBParadiseGate`); das Paradies als schwebende Insel
1,5 km über dem Thron (`ADBParadiseIsland`: Kirschbäume, Ahorn, Pagode, Schreinhaus, Teich, Laternenpfad, eigene
goldene Stimmung, Gebiet „Paradies“); Schrein des Friedens mit Finale-Bildschirm und Credits für alle Spieler auf der
Insel; Rückkehr-Tor in die Hauptstadt; Quest `MQ13_Paradise`; Testbefehle `DBParadiseGate`, `DBParadise`, `DBFinale`.
Details: [PARADISE.md](PARADISE.md).

**GETESTET**: Regelkern 66/66 (neu: Weltreinigung, keine Rudel in befreitem DAS ENDE); headless: König fällt → Welt
gereinigt (alle Gebiete befreit, 0 Rudel) → Pforte öffnet sich → MQ13 → Pforte → Gebiet Paradies → Schrein → MQ13
abgeschlossen → Finale; Rückkehr in die Hauptstadt; Koop: Client geht durch die Pforte, Paradies-Stimmung und Finale auf
dem Client; gerendert: Pforte am Thron, Insel mit Blick über den Kontinent, Finale-Bildschirm, Rückkehr-Tor; 92–100 FPS.
