# Run inside the Unreal Editor Python environment (UE 5.8):
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_import_world_map.py
#
# Imports the world map (docs/VisualPack/Reference/DarkBlood_Weltkarte.png) as the UI texture of the map window [M]:
# /Game/DarkBlood/UI/Map/T_WorldMap. The window shows the continent part of the image; positions are projected
# with DBRealm::ToMapPixel, the same projection the realm layout was built from. Re-running re-imports.
import os
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "docs", "VisualPack", "Reference", "DarkBlood_Weltkarte.png")
DESTINATION = "/Game/DarkBlood/UI/Map"
NAME = "T_WorldMap"


def log(message):
    unreal.log("DBMAP " + message)


def main():
    if not os.path.isfile(SOURCE):
        unreal.log_error("DBMAP missing source image " + SOURCE)
        return
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", SOURCE)
    task.set_editor_property("destination_path", DESTINATION)
    task.set_editor_property("destination_name", NAME)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = unreal.load_asset(DESTINATION + "/" + NAME)
    if not texture:
        unreal.log_error("DBMAP import failed")
        return
    # A full-screen UI image: no mips, never streamed, UI compression (keeps the painted detail).
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    log("imported {} ({} x {})".format(texture.get_path_name(), texture.blueprint_get_size_x(), texture.blueprint_get_size_y()))


main()
