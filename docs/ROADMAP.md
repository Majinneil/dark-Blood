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
| 6 | Open World | ⬜ (Regionen, Gefahrenstufen, Entdeckung als Basis vorhanden) |
| 7 | NPC- und Siedlungssimulation | ⬜ |
| 8 | Reise / Pferde / Kutschen / Survival | ⬜ |
| 9 | Dungeon-System | ⬜ |
| 10 | Boss-Framework | ⬜ |
| 11 | 14 Regionen | ⬜ |
| 12 | 14 Vasallen | ⬜ |
| 13 | DAS ENDE | ⬜ |
| 14 | Dämonenkönig | ⬜ |
| 15 | Paradies & Finale | ⬜ |
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
- Welt: Zwischenboss → umkämpft, Vasall → befreit (idempotent), DAS ENDE nach 14 Vasallen,
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
