# DARK BLOOD – Visual Asset Integration Pack

Ziel: Die vorhandene Phase-5-Greybox von DARK BLOOD in Unreal Engine 5.8 in eine hochwertige, realistische/semi-realistische Dark-Fantasy-Open-World im mittelalterlich-japanischen Stil überführen.

Dieses Paket ist bewusst **kein Sammelsurium aus zufälligen Assets**. Es ist ein kuratierter Produktionsplan für Claude Code / Codex.

## Wichtig

Ich habe nur Quellen aufgenommen, die in eine der folgenden Gruppen fallen:

1. **AI-SAFE / CLAUDE-SAFE** – CC0 oder aktuell als kostenlos + „Allows usage with AI: Yes“ ausgewiesen. Diese Assets dürfen in den Claude-/Codex-Workflow aufgenommen werden, sofern die jeweilige aktuelle Lizenz beim Download unverändert gilt.
2. **MANUAL-ONLY / NO-AI** – hochwertige kostenlose Assets, die aktuell als „Allows usage with AI: No“ ausgewiesen sind. Diese dürfen nicht als Input an Claude/Codex gegeben werden. Sie können manuell in Unreal verwendet werden, wenn ihre Lizenz das erlaubt.
3. **REFERENCE ONLY** – visuelle Inspiration, nicht als Produktionsasset vorgesehen.

## Was du Claude Code gibst

Gib Claude Code den gesamten Inhalt dieser ZIP-Datei plus dein aktuelles DARK-BLOOD-Projekt.

Claude soll zuerst `01_CLAUDE_MASTER_TASK.md` lesen und danach:

- die vorhandene Phase-5-Greybox analysieren,
- eine saubere `/Game/DarkBlood/...` Asset-Struktur anlegen,
- Master Materials und Landscape-Materialien vorbereiten,
- PCG für Vegetation und Set Dressing aufsetzen,
- Greybox-Flächen systematisch mit echten Materialien und modularen Bauteilen ersetzen,
- alle AI-sicheren Assets importieren bzw. für den Import vorbereiten,
- NO-AI-Assets nur als manuelle Slots/Referenzen behandeln,
- keine funktionierende Gameplay-Logik beschädigen.

## Wichtigster Qualitätsgrundsatz

DARK BLOOD soll **nicht nach Asset-Store-Collage** aussehen. Alle Assets müssen über ein gemeinsames Material-, Farb-, Licht- und Maßstabssystem vereinheitlicht werden.

Siehe:
- `04_VISUAL_BIBLE.md`
- `05_REGION_ASSET_MATRIX.md`
- `07_PHASE_5_5_PLAN.md`

## UPDATE – Character, Animation, Buildings & Textures

Das Paket enthält jetzt zusätzlich:
- `12_CHARACTER_ART_PIPELINE.md`
- `13_ANIMATION_PIPELINE.md`
- `14_BUILDING_ART_RECIPE.md`
- `15_TEXTURE_MATERIAL_RECIPE.md`
- `16_FREE_HIGH_END_SOURCES.md`
- `17_CHARACTER_AND_ANIMATION_MANIFEST.csv`
- `18_CLAUDE_VISUAL_CHARACTER_TASK.md`
- `19_VISUAL_ACCEPTANCE_CHECKLIST.md`
- `20_DARK_BLOOD_VISUAL_RECIPE.md`
- `Tools/UE58/`
- `Schemas/`
- `References/`

Für Claude Code ist `18_CLAUDE_VISUAL_CHARACTER_TASK.md` der aktuelle Integrationsauftrag.
