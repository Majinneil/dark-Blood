# Run inside Unreal Editor Python environment (UE 5.8)
# Read-only audit: lists assets under /Game/DarkBlood and flags DEV/placeholder naming.
import unreal

ROOT = "/Game/DarkBlood"
assets = unreal.EditorAssetLibrary.list_assets(ROOT, recursive=True, include_folder=False)

placeholder_tokens = ("dev_", "placeholder", "greybox", "blockout", "temp_")
counts = {}
flagged = []

for path in assets:
    data = unreal.EditorAssetLibrary.find_asset_data(path)
    cls = str(data.asset_class_path.asset_name) if data and data.is_valid() else "Unknown"
    counts[cls] = counts.get(cls, 0) + 1
    low = path.lower()
    if any(token in low for token in placeholder_tokens):
        flagged.append(path)

unreal.log("=== DARK BLOOD VISUAL ASSET AUDIT ===")
for cls, count in sorted(counts.items(), key=lambda x: (-x[1], x[0])):
    unreal.log(f"{cls}: {count}")

unreal.log_warning(f"Placeholder/DEV assets found: {len(flagged)}")
for path in flagged[:500]:
    unreal.log_warning(path)

if len(flagged) > 500:
    unreal.log_warning(f"...and {len(flagged)-500} more")
