# Run inside the Unreal Editor Python environment (UE 5.8):
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_import_sketchfab.py
#
# Imports the free Sketchfab models (CC-BY / CC0, see docs/CREDITS.md) from SourceArt/Sketchfab/<key>/**/scene.gltf to
# /Game/DarkBlood/Art/Environment/Sketchfab/<key>/ with Nanite on, blend materials turned into cut-outs, and writes
# Saved/Sketchfab/manifest.txt: every static mesh with its bounds, so code can scale and ground the models.
# Afterwards run db_fix_material_usage.py (Nanite / instancing usage for the imported glTF materials).
import os
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "SourceArt", "Sketchfab")
ROOT = "/Game/DarkBlood/Art/Environment/Sketchfab"
MANIFEST = os.path.join(PROJECT, "Saved", "Sketchfab", "manifest.txt")

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(message):
    unreal.log("DBSKETCHFAB " + message)


def find_gltf(folder):
    for base, _, files in os.walk(folder):
        for name in files:
            if name.endswith(".gltf") or name.endswith(".glb"):
                return os.path.join(base, name)
    return None


lines = []
only = os.environ.get("DBSF_ONLY", "")
for key in sorted(os.listdir(SOURCE)) if os.path.isdir(SOURCE) else []:
    if only and key not in only.split(","):
        continue
    gltf = find_gltf(os.path.join(SOURCE, key))
    if not gltf:
        log("no glTF in " + key)
        continue
    destination = ROOT + "/" + key
    if library.does_directory_exist(destination):
        library.delete_directory(destination)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", gltf)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    meshes = 0
    for path in library.list_assets(destination, recursive=True, include_folder=False):
        loaded = unreal.load_asset(path)
        if isinstance(loaded, unreal.MaterialInstanceConstant):
            parent = loaded.get_editor_property("parent")
            if parent and "MI_Default_Blend" in parent.get_name():
                masked = unreal.load_asset(parent.get_path_name().replace("MI_Default_Blend", "MI_Default_Mask").split(".")[0])
                if masked:
                    mel.set_material_instance_parent(loaded, masked)
                    mel.update_material_instance(loaded)
        if isinstance(loaded, unreal.StaticMesh):
            settings = loaded.get_editor_property("nanite_settings")
            settings.set_editor_property("enabled", True)
            loaded.set_editor_property("nanite_settings", settings)
            bounds = loaded.get_bounding_box()
            lo, hi = bounds.min, bounds.max
            lines.append("%s|%s|%.1f,%.1f,%.1f|%.1f,%.1f,%.1f|%d" % (key, loaded.get_path_name(), lo.x, lo.y, lo.z, hi.x, hi.y, hi.z,
                                                              loaded.get_num_triangles(0)))
            meshes += 1
        if loaded is not None:
            library.save_loaded_asset(loaded, False)
    log("model %s: %d meshes" % (key, meshes))

os.makedirs(os.path.dirname(MANIFEST), exist_ok=True)
with open(MANIFEST, "w") as out:
    out.write("\n".join(lines) + "\n")
log("manifest %s (%d meshes)" % (MANIFEST, len(lines)))
