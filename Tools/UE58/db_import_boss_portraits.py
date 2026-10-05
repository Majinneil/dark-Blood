# Run inside the Unreal Editor Python environment (UE 5.8):
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_import_boss_portraits.py
#
# Imports the boss portraits (docs/VisualPack/Reference/BossPortraits/T_Portrait_<BossId>.png, 256 x 256, cut from the
# head panels of References/DARK_BLOOD_16_VASALLEN_UND_DAEMONENKOENIG.pdf) as UI textures for the boss bar:
# /Game/DarkBlood/UI/Bosses/T_Portrait_<BossId>. UDBBossDefinition::Portrait points there. Re-running re-imports.
import glob
import os
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "docs", "VisualPack", "Reference", "BossPortraits")
DESTINATION = "/Game/DarkBlood/UI/Bosses"


def main():
    files = sorted(glob.glob(os.path.join(SOURCE, "T_Portrait_*.png")))
    if not files:
        unreal.log_error("DBPORTRAIT no portraits in " + SOURCE)
        return
    tasks = []
    for path in files:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", path)
        task.set_editor_property("destination_path", DESTINATION)
        task.set_editor_property("destination_name", os.path.splitext(os.path.basename(path))[0])
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    imported = 0
    for path in files:
        name = os.path.splitext(os.path.basename(path))[0]
        texture = unreal.load_asset(DESTINATION + "/" + name)
        if not texture:
            unreal.log_error("DBPORTRAIT import failed: " + name)
            continue
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property("never_stream", True)
        texture.set_editor_property("srgb", True)
        unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
        imported += 1
    unreal.log("DBPORTRAIT imported {} of {} portraits to {}".format(imported, len(files), DESTINATION))


main()
