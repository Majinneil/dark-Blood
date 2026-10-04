# Run inside the Unreal Editor Python environment (UE 5.8) after generate_backdrop_terrain.py:
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_import_backdrop_terrain.py
#
# Imports the generated horizon terrain (our own content) to /Game/DarkBlood/Art/Environment/Backdrop with Nanite,
# no collision (far outside the playable area) and the terrain material instances per slot.
import os
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "SourceArt", "Generated")
DESTINATION = "/Game/DarkBlood/Art/Environment/Backdrop"
INSTANCES = "/Game/DarkBlood/Art/Materials/Instances/"

MESHES = {
    "SM_DB_Backdrop_Mountains": {"Rock": "MI_DB_Terrain_Mountain", "Snow": "MI_DB_Terrain_Snow"},
    "SM_DB_Backdrop_Hills": {"Grass": "MI_DB_Terrain_Hills", "Rock": "MI_DB_Terrain_Cliff"},
}

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary


def log(message):
    unreal.log("DBTERRAIN " + message)


for name, slots in MESHES.items():
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(SOURCE, name + ".obj"))
    task.set_editor_property("destination_path", DESTINATION)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    mesh = None
    for path in task.get_editor_property("imported_object_paths"):
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.StaticMesh):
            mesh = asset
    if mesh is None:
        unreal.log_error("DBTERRAIN import failed: " + name)
        continue
    if mesh.get_name() != name:
        library.rename_asset(mesh.get_path_name().split(".")[0], DESTINATION + "/" + name)
        mesh = unreal.load_asset(DESTINATION + "/" + name)
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.set_editor_property("enabled", True)
    mesh.set_editor_property("nanite_settings", nanite)
    # Horizon only: no collision geometry (it would be huge and nobody can reach it).
    body = mesh.get_editor_property("body_setup")
    if body is not None:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        for key, instance_name in slots.items():
            if key.lower() in slot_name.lower():
                material = unreal.load_asset(INSTANCES + instance_name)
                if material is not None:
                    mesh.set_material(index, material)
                    log("%s slot %s -> %s" % (name, slot_name, instance_name))
    library.save_loaded_asset(mesh, False)
    bounds = mesh.get_bounding_box()
    log("%s bounds min %s max %s" % (name, bounds.min, bounds.max))

# Materials created by the OBJ import (one per slot) are unused after the assignment.
for path in library.list_assets(DESTINATION, recursive=False, include_folder=False):
    asset = unreal.load_asset(path)
    if isinstance(asset, unreal.MaterialInterface) and not asset.get_name().startswith("MI_DB"):
        library.delete_asset(path)
log("backdrop terrain import complete")
