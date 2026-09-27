# Roadmap und Status

Legende: ✅ fertig und getestet · 🟡 implementiert, nicht (vollständig) getestet · ⬜ offen

| Phase | Inhalt | Status |
|---|---|---|
| 0 | Preproduction: Analyse, Architektur, Struktur, Konventionen, Dokumentation | ✅ |
| 1 | Technisches Fundament | 🟡 Regelkern ✅ · UE-Schicht geschrieben, **nicht kompiliert** |
| 2 | Character & Combat Vertical Slice | ⬜ |
| 3 | Erster Story Vertical Slice | ⬜ |
| 4 | Klassen & Skilltrees | ⬜ |
| 5 | Inventar / Loot / Crafting | ⬜ (Inventar-, Taschen- und Equipment-Regeln aus Phase 1 vorhanden) |
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
**BUILD:** Regelkern ✅. UE-Module: **kein Build möglich** (keine Engine).
**MULTIPLAYER:** Replikationsdesign implementiert (siehe MULTIPLAYER.md), **nicht getestet**.
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
