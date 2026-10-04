# Plain Python 3 + Blender:  python Tools/prepare_alpha_cards.py [model keys ...]
#
# Finds the alpha-card materials (glTF alphaMode MASK or BLEND with a base color texture) of the Sketchfab models in
# SourceArt/Sketchfab/<key>/scene.gltf, lets headless Blender split each texture's alpha into an opacity map
# (Tools/Blender/split_alpha.py) and writes SourceArt/Sketchfab/cards.json for Tools/UE58/db_fix_sketchfab_materials.py,
# which moves those materials onto the project's foliage master (the imported glTF masked material does not cut them
# out in the game). Without keys every model is processed.
import json
import os
import re
import subprocess
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SKETCHFAB = os.path.join(ROOT, "SourceArt", "Sketchfab")
BLENDER = r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"
SPLIT = os.path.join(ROOT, "Tools", "Blender", "split_alpha.py")
# Models whose cards are plants (they sway in the wind); everything else (thatch, rope) stays still.
PLANTS = ("bamboo", "pine", "cedar", "cherry", "maple", "tree")


def unreal_name(name):
    # Interchange turns characters that are not allowed in asset names into underscores.
    return re.sub(r"[^A-Za-z0-9_]", "_", name)


def main():
    keys = sys.argv[1:] or sorted(os.listdir(SKETCHFAB))
    out_path = os.path.join(SKETCHFAB, "cards.json")
    cards = json.load(open(out_path)) if os.path.isfile(out_path) else []
    cards = [c for c in cards if c["key"] not in keys]
    for key in keys:
        gltf_path = os.path.join(SKETCHFAB, key, "scene.gltf")
        if not os.path.isfile(gltf_path):
            continue
        gltf = json.load(open(gltf_path, encoding="utf-8"))
        images, textures = gltf.get("images", []), gltf.get("textures", [])
        for material in gltf.get("materials", []):
            if material.get("alphaMode", "OPAQUE") == "OPAQUE":
                continue
            base = material.get("pbrMetallicRoughness", {}).get("baseColorTexture")
            if not base:
                continue
            uri = images[textures[base["index"]]["source"]]["uri"]
            if not uri.lower().endswith(".png"):
                continue  # no alpha channel in a JPEG
            normal = material.get("normalTexture")
            prefix = unreal_name(material["name"])
            subprocess.run([BLENDER, "-b", "--python", SPLIT, "--", os.path.join(SKETCHFAB, key, uri),
                            os.path.join(SKETCHFAB, key, "db"), prefix], check=True, capture_output=True)
            cards.append({"key": key, "material": unreal_name(material["name"]), "base": uri,
                          "normal": images[textures[normal["index"]]["source"]]["uri"] if normal else None,
                          "prefix": prefix, "wind": any(word in key for word in PLANTS)})
            print("card", key, material["name"])
    json.dump(cards, open(out_path, "w"), indent=1)
    print("wrote", out_path, len(cards), "cards")


if __name__ == "__main__":
    main()
