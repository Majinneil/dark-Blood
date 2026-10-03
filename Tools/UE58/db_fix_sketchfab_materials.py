# Run inside the Unreal Editor Python environment (UE 5.8) after db_import_sketchfab.py, db_fix_material_usage.py and
# Tools/Blender/split_alpha.py:
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_fix_sketchfab_materials.py
#
# Some Sketchfab models draw thatch and foliage as alpha cards that the imported glTF material does not cut out (the cards
# show white where they should be see-through). Their slot gets an instance of the project's two-sided foliage master
# (M_DB_Foliage_Master) with the alpha split into its own map by Blender, no wind.
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ROOT = "/Game/DarkBlood/Art/Environment/Sketchfab"
OPAQUE = "/Game/DarkBlood/Art/Materials/Imported/DB_MI_Default_Opaque"
# model folder: (alpha-card material slot, base color, normal, split prefix in SourceArt/Sketchfab/<key>/db, opaque slots)
CARDS = {
    "shirakawago_house": ("M_Shirakawago_House_1_Details", "M_Shirakawago_House_1_Details_baseColor.png",
                          "M_Shirakawago_House_1_Details_normal.jpeg", "thatch",
                          ["M_Shirakawago_House_1_1", "M_Shirakawago_House_1_2", "M_Shirakawago_House_1_3"]),
}

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
foliage = unreal.load_asset("/Game/DarkBlood/Art/Materials/Master/M_DB_Foliage_Master")
opaque = unreal.load_asset(OPAQUE)


def log(message):
    unreal.log("DBSFMAT " + message)


def import_texture(filename, destination, name, linear, normal=False):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    tools.import_asset_tasks([task])
    texture = unreal.load_asset(destination + "/" + name)
    if normal:
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("flip_green_channel", True)  # glTF normals are OpenGL style
    texture.set_editor_property("srgb", not linear)
    library.save_loaded_asset(texture, False)
    return texture


for key, (slot_name, base, normal, prefix, opaque_slots) in CARDS.items():
    source = os.path.join(PROJECT, "SourceArt", "Sketchfab", key)
    destination = ROOT + "/" + key + "/DB"
    maps = {
        "D": import_texture(os.path.join(source, "textures", base), destination, "T_%s_D" % prefix, False),
        "N": import_texture(os.path.join(source, "textures", normal), destination, "T_%s_N" % prefix, True, True),
        "ARM": import_texture(os.path.join(source, "db", prefix + "_arm.png"), destination, "T_%s_ARM" % prefix, True),
        "A": import_texture(os.path.join(source, "db", prefix + "_alpha.png"), destination, "T_%s_A" % prefix, True),
    }
    # The imported card instance itself is re-parented to the foliage master (the meshes keep their slots).
    replaced = 0
    for asset_path in library.list_assets(ROOT + "/" + key, recursive=True, include_folder=False):
        asset = unreal.load_asset(asset_path)
        if not isinstance(asset, unreal.MaterialInstanceConstant):
            continue
        if asset.get_name() in opaque_slots and opaque:
            # Undo an earlier masked switch: these slots are solid wood and plaster.
            mel.set_material_instance_parent(asset, opaque)
        elif asset.get_name() == slot_name and foliage:
            mel.clear_all_material_instance_parameters(asset)
            mel.set_material_instance_parent(asset, foliage)
            mel.set_material_instance_static_switch_parameter_value(asset, "UseTextures", True)
            mel.set_material_instance_static_switch_parameter_value(asset, "WorldAligned", False)
            mel.set_material_instance_texture_parameter_value(asset, "T_BaseColor", maps["D"])
            mel.set_material_instance_texture_parameter_value(asset, "T_Normal", maps["N"])
            mel.set_material_instance_texture_parameter_value(asset, "T_ORM", maps["ARM"])
            mel.set_material_instance_texture_parameter_value(asset, "T_Opacity", maps["A"])
            mel.set_material_instance_vector_parameter_value(asset, "TextureTint", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
            mel.set_material_instance_scalar_parameter_value(asset, "MacroStrength", 0.0)
            mel.set_material_instance_scalar_parameter_value(asset, "WindIntensity", 0.0)  # a roof does not wave
            replaced += 1
        else:
            continue
        mel.update_material_instance(asset)
        library.save_loaded_asset(asset, False)
    log("%s: %s -> foliage master (%d)" % (key, slot_name, replaced))
