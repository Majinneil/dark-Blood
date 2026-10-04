# Reisen und Survival

**Status:** Phase 8 ✅ Gameplay – Pferd, Kutschen-Schnellreise, leichtes Survival, Schwimm-Ausdauer; Regelkern-Tests,
Headless-, Koop- und gerenderte Tests. Offen: Pferde-/Kutschenmodelle und Reit-Animationen (Assets), Kutschen als
fahrende Fahrzeuge auf Straßen, Lagerfeuer, Survival-Werte im Spielstand (werden beim Laden voll).

## Pferd (`ADBHorse`)

- `[H]` ruft das eigene Pferd (beim ersten Mal erscheint es) an einen freien Platz in 6–12 m Nähe (nicht auf Dächern,
  nicht in Häusern oder Bäumen); `[E]` steigt auf / ab (nur der Besitzer).
- Wie bei den Schiffen bleibt der Reiter er selbst (Kamera, Fähigkeiten, Speichern) und hängt am Sattel; seine
  Bewegungseingabe lenkt das Pferd auf dem Server relativ zur Kamera. Trab 9 m/s, `Shift` Galopp 15 m/s mit eigener
  Pferde-Ausdauer (−12/s, +8/s). Im Sattel keine Angriffe, Ausweichen oder Fähigkeiten; Fenster und Interaktion gehen.
- Schwimmt (`UDBCharacterMovementComponent`). Platzhalterkörper aus Grundformen.

## Kutschen (`ADBCarriageStation`)

- Eine Station am Rand jeder der 17 Siedlungen (vom `ADBRealmDirector` auf dem Server aufgestellt).
- `[E]` öffnet die Zielliste (Fahrpreis 10 Mon + 6 Mon/km, Dauer 0,5 h + Strecke / 20 km/h). Der Server prüft Abstand
  (≤ 8 m) und Mon, zieht den Fahrpreis ab und setzt den Spieler neben die Zielstation. Allein vergeht die Reisezeit, im
  Koop nicht. Ein gerittenes Pferd bleibt zurück (rufbar).

## Survival (`DarkBloodRules/Survival.h`, `UDBSurvivalComponent` am PlayerState)

| Wert | Verlauf | Wirkung |
|---|---|---|
| Sättigung 0–100 | −3,5 pro Spielstunde (voll → leer ≈ 1 Spieltag); Essen füllt (Onigiri +35) | > 75 gut genährt: Regeneration +10 % · < 25 hungrig: Ausdauer-Reg. ×0,75, Leben ×0,5 · < 5: keine Heilung, Ausdauer ×0,5 |
| Wärme 0–100 | Kälte = Region (Eisöde 0,8, Nebelberge 0,4, Festung 0,15) + Höhe über 600 m, nachts +0,15, nass +0,3, × (1 − Frostschutz); −60/h bei Kälte 1; Siedlungen +120/h; sonst +20/h | < 40 kalt: Ausdauer-Reg. ×0,8 · < 10 erfrierend: ×0,6, keine Heilung |

Kein Schaden durch Hunger oder Kälte („keine Frustmechaniken“). **Schwimmen** kostet 5 Ausdauer/s (Regeneration
pausiert, Tag `State.Swimming`); erschöpft verliert man 4 % Leben/s und kann ertrinken (normale Todesstrafe).
HUD: Balken „Sättigung“ und „Wärme“; Meldungen beim Wechsel des Zustands.

## Getestet

Regelkern 54/54 (Hunger bremst, schadet nie; Kälte/Siedlungswärme; Essen). Headless: gut genährt 21,8/s, hungrig
14,9/s, Onigiri 10 → 45, Eisöde kalt (×0,8), Schwimmen bis zur Erschöpfung (Leben 292 → 194). Kutsche: zu weit
abgelehnt, Hauptstadt → Dorf (28 Mon, 0,65 h), zu teuer abgelehnt. Pferd: Trab 9 m/s, Galopp 15 m/s (Ausdauer 100 → 18),
Rufen neben Häusern. Koop: Client fährt Kutsche (keine Zeit vergeht), reitet und galoppiert, Server bestätigt.

## Testbefehle

`DBRide`, `DBWalk <Gierwinkel> <s>` (lenkt im Sattel das Pferd), `DBInput Sprint <s>`, `DBCarriage <Siedlung>`,
`DBSurvival <Sättigung> <Wärme>`, `DBDumpCharacter` (Survival-Zeile).
