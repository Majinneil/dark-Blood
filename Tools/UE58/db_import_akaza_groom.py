# Run inside the Unreal Editor Python environment (UE 5.8, plugin HairStrands):
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_import_akaza_groom.py
#
# Imports Akaza's strand hair (Tools/Art/blender_make_akaza_hair.py -> SourceArt/Hyper3D/Akaza/export/Akaza_Hair.abc,
# objects Hair and Beard, centimetres, Blender axes) as two grooms next to his body:
#   /Game/DarkBlood/Characters/Hyper3D/Akaza/GR_Akaza_Hair (hair and beard; Unreal makes one group of the two objects)
# and the hair material /Game/DarkBlood/Art/Materials/Master/M_DB_Hair (Hair shading model, dark brown-black).
# The game puts them on Akaza's head bone (DBDevelopmentContent, DBCharacterVisualComponent::ApplyGrooms).
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ABC = os.path.join(PROJECT, "SourceArt", "Hyper3D", "Akaza", "export", "Akaza_Hair.abc")
TARGET = "/Game/DarkBlood/Characters/Hyper3D/Akaza"
MATERIAL = "/Game/DarkBlood/Art/Materials/Master/M_DB_Hair"
WIDTHS = {"Hair": 0.007, "Beard": 0.009}          # strand width in cm

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(message):
    unreal.log("DBGROOM " + message)


def hair_material():
    folder, name = MATERIAL.rsplit("/", 1)
    material = unreal.load_asset(MATERIAL)
    if material:
        for expression in list(mel.get_material_expressions(material)):
            mel.delete_material_expression(material, expression)
    else:
        material = tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_HAIR)
    material.set_editor_property("used_with_hair_strands", True)
    colour = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -500, -200)
    colour.set_editor_property("parameter_name", "HairColor")
    colour.set_editor_property("default_value", unreal.LinearColor(0.045, 0.03, 0.022, 1.0))
    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, 50)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.42)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, 200)
    spec.set_editor_property("parameter_name", "Specular")
    spec.set_editor_property("default_value", 0.35)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(material)
    library.save_loaded_asset(material, False)
    log("material " + MATERIAL)
    return material


def import_groom():
    options = unreal.GroomImportOptions()
    conversion = options.get_editor_property("conversion_settings")
    # Alembic from Blender is Y-up and right-handed: rotate X 90 and mirror Y gives Unreal's space - the same mapping the
    # FBX import applies to the body, so hair and body line up.
    conversion.set_editor_property("rotation", unreal.Vector(90.0, 0.0, 0.0))
    conversion.set_editor_property("scale", unreal.Vector(1.0, -1.0, 1.0))
    options.set_editor_property("conversion_settings", conversion)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", ABC)
    task.set_editor_property("destination_path", TARGET)
    task.set_editor_property("destination_name", "GR_Akaza_Hair")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    task.set_editor_property("options", options)
    # The generic Alembic importer would take the file as a static mesh: ask for the strand-hair factory explicitly.
    task.set_editor_property("factory", unreal.HairStrandsFactory())
    tools.import_asset_tasks([task])
    paths = list(task.get_editor_property("imported_object_paths"))
    log("imported %s" % paths)
    return [unreal.load_asset(p) for p in paths]


if not os.path.isfile(ABC):
    raise SystemExit("no hair - run Tools/Art/blender_make_akaza_hair.py first")
material = hair_material()
grooms = [g for g in import_groom() if isinstance(g, unreal.GroomAsset)]
for groom in grooms:
    groups = groom.get_editor_property("hair_groups_info")
    names = [str(g.get_editor_property("group_name")) if hasattr(g, "group_name") else "" for g in groups]
    log("%s: %d groups %s" % (groom.get_name(), len(groups), names))
    rendering = groom.get_editor_property("hair_groups_rendering")
    for index, settings in enumerate(rendering):
        geometry = settings.get_editor_property("geometry_settings")
        geometry.set_editor_property("hair_width", WIDTHS["Beard"] if index == 1 else WIDTHS["Hair"])
        settings.set_editor_property("geometry_settings", geometry)
    groom.set_editor_property("hair_groups_rendering", rendering)
    library.save_loaded_asset(groom, False)
log("done")
