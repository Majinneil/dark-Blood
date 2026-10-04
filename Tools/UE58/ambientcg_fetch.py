# Downloads the DARK BLOOD ambientCG selection (CC0: https://ambientcg.com/license) into SourceArt/AmbientCG.
# Plain Python 3:  python Tools/UE58/ambientcg_fetch.py [--res 2K]
# Imported together with the Poly Haven set by Tools/UE58/db_import_polyhaven.py.
import argparse
import io
import pathlib
import urllib.request
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "SourceArt" / "AmbientCG"

MATERIALS = [
    "PaintedWood003",  # worn vermilion lacquer: torii, temple posts, bridge rails
    "PaintedWood005",  # worn black lacquer: torii kasagi, trims
    "Paper001",        # shoji paper
    "Paper004",        # lantern paper
    "Fabric036",       # linen
    "Fabric026",       # crimson cloth (tavern noren)
    "Fabric023",       # indigo cloth (merchant noren)
    "Metal009",        # dark iron: anvil, fittings
    "Metal035",        # bronze: bridge post caps
    "Bamboo002A",      # bamboo culms and fences
]
# ambientCG map suffix -> short name used by the importer
MAPS = {"Color": "diff", "NormalGL": "nor_gl", "Roughness": "rough"}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--res", default="2K")
    args = parser.parse_args()
    total = 0
    for asset in MATERIALS:
        folder = OUT / asset
        wanted = {folder / f"{asset}_{short}_{args.res}.jpg": key for key, short in MAPS.items()}
        if all(path.exists() for path in wanted):
            continue
        url = f"https://ambientcg.com/get?file={asset}_{args.res}-JPG.zip"
        request = urllib.request.Request(url, headers={"User-Agent": "DarkBlood-AssetFetch"})
        data = urllib.request.urlopen(request).read()
        total += len(data)
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            for path, key in wanted.items():
                member = next((n for n in archive.namelist() if n.endswith(f"_{key}.jpg")), None)
                if member is None:
                    print("missing", key, "in", asset)
                    continue
                folder.mkdir(parents=True, exist_ok=True)
                path.write_bytes(archive.read(member))
        print("material", asset)
    (OUT / "LICENSE.txt").parent.mkdir(parents=True, exist_ok=True)
    (OUT / "LICENSE.txt").write_text("All assets in this folder are CC0 (public domain) from https://ambientcg.com (license: https://docs.ambientcg.com/license/).\n")
    print("downloaded %.1f MB" % (total / 1e6))


if __name__ == "__main__":
    main()
