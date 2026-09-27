# Downloads the DARK BLOOD Poly Haven selection (CC0, AI-safe: https://polyhaven.com/license) into SourceArt/PolyHaven.
# Plain Python 3 (no Unreal needed):  python Tools/UE58/polyhaven_fetch.py [--res 2k]
# Then import into Unreal:  UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_import_polyhaven.py
#
# Textures: diffuse (sRGB), normal (OpenGL; flipped to DirectX on import), ARM (AO/Rough/Metal packed = T_ORM).
# Models: glTF with textures (rocks, shrubs, ferns, trees) for the scatter volumes.
import argparse
import json
import pathlib
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "SourceArt" / "PolyHaven"

# slot -> Poly Haven id (see docs/VISUAL_FOUNDATION_STATUS.md for the reasoning)
TEXTURES = [
    "weathered_planks",      # dark weathered wood: posts, beams, plank walls
    "hinoki_planks",         # light Japanese cypress: verandas, tables, new wood
    "old_planks_02",         # old / wet grey wood: bridge deck
    "plastered_wall_02",     # white lime plaster (shikkui)
    "clay_plaster",          # earthen clay plaster (tsuchikabe), villages
    "grey_roof_01",          # dark fired roof tiles (kawara)
    "reed_roof_04",          # thatch
    "japanese_stone_wall",   # plinths, castle wall base, stairs, lanterns
    "mossy_rock",            # village plinths, boulders
    "lichen_rock",           # mountain rock, dungeon mound
    "rock_pitted_mossy",     # ruins, dungeon door frame
    "grey_stone_path",       # capital courtyard paving
    "forest_leaves_04",      # forest floor
    "rocky_trail_02",        # roads, packed earth
    "japanese_cedar_bark",   # tree bark
    "sakura_bark",           # cherry bark
    "burned_ground_01",      # Dark Blood corrupted soil
]
TEXTURE_MAPS = {"Diffuse": "diff", "nor_gl": "nor_gl", "arm": "arm"}

MODELS = [
    "boulder_01",
    "rock_moss_set_01",
    "rock_moss_set_02",
    "shrub_02",
    "shrub_04",
    "fern_02",
    "moss_01",
    "tree_stump_01",
    "dead_tree_trunk_02",
    "tree_small_02",  # ~100 MB; fir_tree_01 / pine_tree_01 are 0.5-1 GB each and left out
    "island_tree_02",  # gnarled old tree: forest variety and (with blossom leaves) the cherry tree
    "fir_sapling_medium",  # young firs for mountain forests
]


def fetch_json(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "DarkBlood-AssetFetch"})) as response:
        return json.load(response)


def download(url, target, size):
    if target.exists() and target.stat().st_size == size:
        return 0
    target.parent.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "DarkBlood-AssetFetch"})) as response:
        target.write_bytes(response.read())
    return size


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--res", default="2k")
    parser.add_argument("--model-res", default="1k")
    parser.add_argument("--textures-only", action="store_true")
    args = parser.parse_args()

    total = 0
    for asset in TEXTURES:
        files = fetch_json("https://api.polyhaven.com/files/" + asset)
        for key, short in TEXTURE_MAPS.items():
            entry = files[key][args.res]["jpg"]
            total += download(entry["url"], OUT / "Textures" / asset / f"{asset}_{short}_{args.res}.jpg", entry["size"])
        print("texture", asset)
    if not args.textures_only:
        for asset in MODELS:
            files = fetch_json("https://api.polyhaven.com/files/" + asset)
            gltf = files["gltf"][args.model_res]["gltf"]
            folder = OUT / "Models" / asset
            total += download(gltf["url"], folder / pathlib.Path(gltf["url"]).name, gltf["size"])
            for relative, entry in gltf.get("include", {}).items():
                total += download(entry["url"], folder / relative, entry["size"])
            # Leaf / card alpha is a separate map that the glTF does not reference.
            for key, maps in files.items():
                if "alpha" in key.lower() and args.model_res in maps and "jpg" in maps[args.model_res]:
                    entry = maps[args.model_res]["jpg"]
                    total += download(entry["url"], folder / "textures" / pathlib.Path(entry["url"]).name, entry["size"])
            print("model", asset)
    (OUT / "LICENSE.txt").write_text("All assets in this folder are CC0 (public domain) from https://polyhaven.com (license: https://polyhaven.com/license).\n")
    print("downloaded %.1f MB" % (total / 1e6))


if __name__ == "__main__":
    main()
