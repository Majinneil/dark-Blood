# Run inside the Unreal Editor Python environment (UE 5.8) after Tools/generate_sea_normal.py:
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_create_sea_material.py
#
# The open world's sea (M_DB_Sea): a deep, glossy, opaque surface whose chop comes from three world-projected samples of
# the generated wave normal map drifting in different directions (no tiling visible from the coast or a ship). Opaque
# and unlit-free on purpose: the sea covers half the screen from a ship, so it must be cheap. MI_DB_Water_Sea (used by
# the sea plane of L_Realm) is re-parented to it, so the map needs no rebuild.
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "SourceArt", "Generated", "T_DB_Sea_N.png")
TEXTURE_FOLDER = "/Game/DarkBlood/Art/Textures/Generated"
MATERIAL_FOLDER = "/Game/DarkBlood/Art/Materials/Master"
INSTANCE = "/Game/DarkBlood/Art/Materials/Instances/MI_DB_Water_Sea"

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
E = unreal


def log(message):
    unreal.log("DBSEA " + message)


def node(material, cls, x, y, **props):
    expression = mel.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def link(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError("link failed: %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def binary(material, cls, a, b, x, y, a_out="", b_out=""):
    expression = node(material, cls, x, y)
    link(a, a_out, expression, "A")
    link(b, b_out, expression, "B")
    return expression


def scalar(material, name, value, x, y):
    return node(material, E.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def color(material, name, value, x, y):
    parameter = node(material, E.MaterialExpressionVectorParameter, x, y, parameter_name=name,
                     default_value=unreal.LinearColor(value[0], value[1], value[2], 1.0))
    mask = node(material, E.MaterialExpressionComponentMask, x + 220, y, r=True, g=True, b=True, a=False)
    link(parameter, "", mask, "")
    return mask


# ---- 1. Wave normal map ---------------------------------------------------------------------------------------------
if not os.path.isfile(SOURCE):
    raise RuntimeError("missing %s (run Tools/generate_sea_normal.py)" % SOURCE)
task = unreal.AssetImportTask()
task.set_editor_property("filename", SOURCE)
task.set_editor_property("destination_path", TEXTURE_FOLDER)
task.set_editor_property("destination_name", "T_DB_Sea_N")
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
tools.import_asset_tasks([task])
texture = unreal.load_asset(TEXTURE_FOLDER + "/T_DB_Sea_N")
texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
texture.set_editor_property("srgb", False)
texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
library.save_loaded_asset(texture, False)
log("texture T_DB_Sea_N")

# ---- 2. Master material ---------------------------------------------------------------------------------------------
path = MATERIAL_FOLDER + "/M_DB_Sea"
if library.does_asset_exist(path):
    library.delete_asset(path)
material = tools.create_asset("M_DB_Sea", MATERIAL_FOLDER, unreal.Material, unreal.MaterialFactoryNew())
material.set_editor_property("used_with_static_lighting", False)

position = node(material, E.MaterialExpressionWorldPosition, -2200, 0)
world_xy = node(material, E.MaterialExpressionComponentMask, -2000, 0, r=True, g=True, b=False, a=False)
link(position, "", world_xy, "")
time = node(material, E.MaterialExpressionTime, -2200, 300)
strength = scalar(material, "WaveStrength", 0.55, -600, 700)

# (tile size cm, drift cm/s x, drift cm/s y, weight): long swell, wind chop, fine ripples
LAYERS = [(5200.0, 32.0, 14.0, 0.8), (1700.0, -26.0, 38.0, 0.6), (520.0, 44.0, -20.0, 0.35)]
total = None
for index, (tile, drift_x, drift_y, weight) in enumerate(LAYERS):
    y = index * 260
    drift = node(material, E.MaterialExpressionConstant2Vector, -2000, 300 + y, r=drift_x, g=drift_y)
    moved = binary(material, E.MaterialExpressionMultiply, time, drift, -1800, 300 + y)
    shifted = binary(material, E.MaterialExpressionAdd, world_xy, moved, -1600, y)
    uv = binary(material, E.MaterialExpressionDivide, shifted,
                node(material, E.MaterialExpressionConstant, -1800, 120 + y, r=tile), -1400, y)
    sample = node(material, E.MaterialExpressionTextureSample, -1200, y, texture=texture,
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    link(uv, "", sample, "UVs")
    slope = node(material, E.MaterialExpressionComponentMask, -950, y, r=True, g=True, b=False, a=False)
    link(sample, "RGB", slope, "")
    weighted = binary(material, E.MaterialExpressionMultiply, slope,
                      node(material, E.MaterialExpressionConstant, -950, 120 + y, r=weight), -750, y)
    total = weighted if total is None else binary(material, E.MaterialExpressionAdd, total, weighted, -550, y)

scaled = binary(material, E.MaterialExpressionMultiply, total, strength, -350, 300)
normal = binary(material, E.MaterialExpressionAppendVector, scaled,
                node(material, E.MaterialExpressionConstant, -350, 450, r=1.0), -150, 300)
normalized = node(material, E.MaterialExpressionNormalize, 50, 300)
link(normal, "", normalized, "")
mel.connect_material_property(normalized, "", unreal.MaterialProperty.MP_NORMAL)

# Colour: deep water, a touch lighter and greener where the chop faces the viewer.
deep = color(material, "DeepColor", (0.004, 0.018, 0.026), -600, -500)
crest = color(material, "CrestColor", (0.012, 0.05, 0.058), -600, -350)
fresnel = node(material, E.MaterialExpressionFresnel, -350, -250, exponent=3.0, base_reflect_fraction=0.0)
link(normalized, "", fresnel, "Normal")
facing = node(material, E.MaterialExpressionOneMinus, -150, -250)
link(fresnel, "", facing, "")
base = node(material, E.MaterialExpressionLinearInterpolate, 50, -400)
link(deep, "", base, "A")
link(crest, "", base, "B")
link(facing, "", base, "Alpha")
mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(scalar(material, "Roughness", 0.06, -200, 100), "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(scalar(material, "Specular", 0.6, -200, 180), "", unreal.MaterialProperty.MP_SPECULAR)

mel.recompile_material(material)
library.save_loaded_asset(material, False)
log("material M_DB_Sea")

# ---- 3. The sea plane's instance ------------------------------------------------------------------------------------
instance = unreal.load_asset(INSTANCE)
if instance is None:
    raise RuntimeError("missing " + INSTANCE)
mel.set_material_instance_parent(instance, material)
mel.clear_all_material_instance_parameters(instance)
mel.update_material_instance(instance)
library.save_loaded_asset(instance, False)
log("instance MI_DB_Water_Sea -> M_DB_Sea")
