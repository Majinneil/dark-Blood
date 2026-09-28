# Run inside the Unreal Editor Python environment (UE 5.8):
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_import_fab_weapons.py
#
# Katana collection from the free Fab "Corrupted Dark Katana" (Deepanshu, Standard license, "Allows usage with AI: Yes").
# Export first with Tools/UE58/blender_export_katana.py. Everything lands in /Game/DarkBlood/Dev/FabWeapons (NOT committed:
# Fab forbids standalone redistribution; every developer imports it from their own library).
#   SM_Katana_Corrupted        one blade model (pivot = grip center, blade +Z)
#   M_DB_Katana_Master         recolors the corruption splatter (emission mask) and its glow per finish
#   MI_DB_Katana_<Finish>      one finish per katana item (see DBDevelopmentContent.cpp, KatanaVisual)
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
EXPORT = os.path.join(PROJECT, "SourceArt", "Fab", "corrupted_dark_katana", "export")
FOLDER = "/Game/DarkBlood/Dev/FabWeapons/Katana"
TEXTURES = FOLDER + "/Textures"

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
E = unreal


def log(message):
    unreal.log("DBKATANA " + message)


def run_import(filename, destination, name=""):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    return list(task.get_editor_property("imported_object_paths"))


# ---- 1. textures + mesh --------------------------------------------------------------------------------------
LINEAR = {"T_Katana_MetallicSmoothness", "T_Katana_AO", "T_Katana_Normal"}
textures = {}
for name in ("T_Katana_Albedo", "T_Katana_Normal", "T_Katana_MetallicSmoothness", "T_Katana_AO", "T_Katana_Emission"):
    run_import(os.path.join(EXPORT, name + ".png"), TEXTURES, name)
    texture = unreal.load_asset(TEXTURES + "/" + name)
    if texture is None:
        raise RuntimeError("texture import failed: " + name)
    texture.set_editor_property("srgb", name not in LINEAR)
    if name == "T_Katana_Normal":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    library.save_loaded_asset(texture, False)
    textures[name] = texture

run_import(os.path.join(EXPORT, "SM_Katana_Corrupted.fbx"), FOLDER, "SM_Katana_Corrupted")
mesh = unreal.load_asset(FOLDER + "/SM_Katana_Corrupted")
if not isinstance(mesh, unreal.StaticMesh):
    # Interchange may name the mesh after the FBX node: pick up whatever static mesh landed in the folder.
    for path in library.list_assets(FOLDER, recursive=False, include_folder=False):
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.StaticMesh):
            if asset.get_name() != "SM_Katana_Corrupted":
                library.rename_asset(path, FOLDER + "/SM_Katana_Corrupted")
            mesh = unreal.load_asset(FOLDER + "/SM_Katana_Corrupted")
            break
if not isinstance(mesh, unreal.StaticMesh):
    raise RuntimeError("katana mesh import failed")
bounds = mesh.get_bounding_box()
log("mesh bounds min=%s max=%s" % (bounds.min, bounds.max))


# ---- 2. master material --------------------------------------------------------------------------------------
def node(material, cls, x, y, **props):
    expression = mel.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def link(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError("link failed: %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def scalar(material, name, value, x, y):
    return node(material, E.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def vector_rgb(material, name, value, x, y):
    parameter = node(material, E.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=value)
    mask = node(material, E.MaterialExpressionComponentMask, x + 200, y, r=True, g=True, b=True, a=False)
    link(parameter, "", mask, "")
    return mask


def binary(material, cls, a, b, x, y, a_out="", b_out=""):
    expression = node(material, cls, x, y)
    link(a, a_out, expression, "A")
    link(b, b_out, expression, "B")
    return expression


master_path = FOLDER + "/M_DB_Katana_Master"
master = unreal.load_asset(master_path) or tools.create_asset("M_DB_Katana_Master", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
mel.delete_all_material_expressions(master)
S = unreal.MaterialSamplerType


def texture_param(name, sampler, x, y):
    return node(master, E.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name=name, texture=textures[name], sampler_type=sampler)


albedo = texture_param("T_Katana_Albedo", S.SAMPLERTYPE_COLOR, -1800, -600)
normal = texture_param("T_Katana_Normal", S.SAMPLERTYPE_NORMAL, -1800, -250)
masks = texture_param("T_Katana_MetallicSmoothness", S.SAMPLERTYPE_LINEAR_COLOR, -1800, 50)
ao = texture_param("T_Katana_AO", S.SAMPLERTYPE_LINEAR_COLOR, -1800, 350)
emission = texture_param("T_Katana_Emission", S.SAMPLERTYPE_COLOR, -1800, 650)

# Corruption mask = brightness of the emission map; AccentAmount recolors those areas (0 = original red finish).
emission_gray = node(master, E.MaterialExpressionDesaturation, -1400, 650)
link(emission, "RGB", emission_gray, "")
mask_gain = scalar(master, "MaskGain", 3.0, -1400, 800)
mask = node(master, E.MaterialExpressionSaturate, -1000, 650)
link(binary(master, E.MaterialExpressionMultiply, emission_gray, mask_gain, -1200, 650), "", mask, "")
accent_amount = scalar(master, "AccentAmount", 0.0, -1000, 800)
recolor = binary(master, E.MaterialExpressionMultiply, mask, accent_amount, -800, 650)

albedo_gray = node(master, E.MaterialExpressionDesaturation, -1400, -600)
link(albedo, "RGB", albedo_gray, "")
accent_tint = vector_rgb(master, "AccentTint", unreal.LinearColor(1.0, 0.1, 0.05, 1.0), -1600, -800)
accent_boost = scalar(master, "AccentBoost", 2.0, -1200, -800)
accent_color = binary(master, E.MaterialExpressionMultiply, binary(master, E.MaterialExpressionMultiply, albedo_gray, accent_tint, -1100, -650), accent_boost,
                      -900, -650)
base_color = node(master, E.MaterialExpressionLinearInterpolate, -600, -600)
link(albedo, "RGB", base_color, "A")
link(accent_color, "", base_color, "B")
link(recolor, "", base_color, "Alpha")
steel = vector_rgb(master, "SteelTint", unreal.LinearColor(1.0, 1.0, 1.0, 1.0), -800, -900)
final_color = binary(master, E.MaterialExpressionMultiply, base_color, steel, -400, -600)

roughness = node(master, E.MaterialExpressionOneMinus, -1400, 100)
link(masks, "A", roughness, "")

# Glow: original emission, or its brightness in GlowColor; slow pulse like the Dark Blood veins.
glow_color = vector_rgb(master, "GlowColor", unreal.LinearColor(1.0, 0.1, 0.05, 1.0), -1600, 950)
tinted_glow = binary(master, E.MaterialExpressionMultiply, emission_gray, glow_color, -1100, 950)
glow = node(master, E.MaterialExpressionLinearInterpolate, -800, 900)
link(emission, "RGB", glow, "A")
link(tinted_glow, "", glow, "B")
link(accent_amount, "", glow, "Alpha")
time = node(master, E.MaterialExpressionTime, -1400, 1150)
pulse_speed = scalar(master, "PulseSpeed", 1.5, -1400, 1250)
sine = node(master, E.MaterialExpressionSine, -1000, 1150, period=6.283)
link(binary(master, E.MaterialExpressionMultiply, time, pulse_speed, -1200, 1150), "", sine, "")
pulse = node(master, E.MaterialExpressionLinearInterpolate, -800, 1150)
link(scalar(master, "PulseMin", 0.45, -1000, 1300), "", pulse, "A")
link(node(master, E.MaterialExpressionConstant, -1000, 1400, r=1.0), "", pulse, "B")
half = binary(master, E.MaterialExpressionMultiply, sine, node(master, E.MaterialExpressionConstant, -1000, 1500, r=0.5), -900, 1450)
link(binary(master, E.MaterialExpressionAdd, half, node(master, E.MaterialExpressionConstant, -900, 1550, r=0.5), -850, 1500), "", pulse, "Alpha")
glow_strength = scalar(master, "GlowStrength", 8.0, -600, 1100)
emissive = binary(master, E.MaterialExpressionMultiply, binary(master, E.MaterialExpressionMultiply, glow, glow_strength, -500, 950), pulse, -300, 950)

P = unreal.MaterialProperty
mel.connect_material_property(final_color, "", P.MP_BASE_COLOR)
mel.connect_material_property(normal, "RGB", P.MP_NORMAL)
mel.connect_material_property(masks, "R", P.MP_METALLIC)
mel.connect_material_property(roughness, "", P.MP_ROUGHNESS)
mel.connect_material_property(ao, "R", P.MP_AMBIENT_OCCLUSION)
mel.connect_material_property(emissive, "", P.MP_EMISSIVE_COLOR)
mel.recompile_material(master)
library.save_loaded_asset(master, False)
log("master compiled")


# ---- 3. one finish per katana --------------------------------------------------------------------------------
def rgb(r, g, b):
    return unreal.LinearColor(r, g, b, 1.0)


# name: (accent tint, accent amount, glow color, glow strength, pulse speed, steel tint)
FINISHES = {
    "Steel": (rgb(0.55, 0.57, 0.6), 1.0, rgb(0, 0, 0), 0.0, 1.0, rgb(1.0, 1.0, 1.05)),
    "Tamahagane": (rgb(0.7, 0.66, 0.55), 1.0, rgb(1.0, 0.7, 0.4), 0.4, 0.6, rgb(1.05, 1.0, 0.95)),
    "Homura": (rgb(1.0, 0.35, 0.05), 1.0, rgb(1.0, 0.32, 0.04), 30.0, 2.2, rgb(1.0, 0.9, 0.85)),
    "Yukiore": (rgb(0.55, 0.8, 1.0), 1.0, rgb(0.3, 0.65, 1.0), 20.0, 0.8, rgb(0.9, 0.95, 1.1)),
    "Dokuga": (rgb(0.35, 0.9, 0.15), 1.0, rgb(0.3, 1.0, 0.08), 18.0, 1.2, rgb(0.9, 1.0, 0.9)),
    "Raikiri": (rgb(0.75, 0.6, 1.0), 1.0, rgb(0.62, 0.52, 1.0), 35.0, 9.0, rgb(0.95, 0.95, 1.1)),
    "Chishio": (rgb(0.7, 0.02, 0.02), 1.0, rgb(1.0, 0.02, 0.02), 14.0, 1.0, rgb(1.0, 0.9, 0.9)),
    "Kagekiri": (rgb(0.25, 0.05, 0.4), 1.0, rgb(0.45, 0.08, 0.95), 12.0, 0.7, rgb(0.7, 0.7, 0.8)),
    "Reiha": (rgb(0.85, 0.95, 1.0), 1.0, rgb(0.7, 0.95, 1.0), 22.0, 1.3, rgb(1.05, 1.05, 1.1)),
    "Kegare": (rgb(1.0, 0.1, 0.05), 0.0, rgb(1.0, 0.1, 0.05), 20.0, 1.5, rgb(1.0, 1.0, 1.0)),
}
for finish, (tint, amount, glow_rgb, strength, speed, steel_rgb) in FINISHES.items():
    name = "MI_DB_Katana_" + finish
    instance = unreal.load_asset(FOLDER + "/" + name) or tools.create_asset(name, FOLDER, unreal.MaterialInstanceConstant,
                                                                               unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, master)
    mel.set_material_instance_vector_parameter_value(instance, "AccentTint", tint)
    mel.set_material_instance_scalar_parameter_value(instance, "AccentAmount", amount)
    mel.set_material_instance_vector_parameter_value(instance, "GlowColor", glow_rgb)
    mel.set_material_instance_scalar_parameter_value(instance, "GlowStrength", strength)
    mel.set_material_instance_scalar_parameter_value(instance, "PulseSpeed", speed)
    mel.set_material_instance_vector_parameter_value(instance, "SteelTint", steel_rgb)
    mel.update_material_instance(instance)
    library.save_loaded_asset(instance, False)
    log("finish " + name)

mesh.set_material(0, unreal.load_asset(FOLDER + "/MI_DB_Katana_Kegare"))
library.save_loaded_asset(mesh, False)
for path in library.list_assets(FOLDER, recursive=True, include_folder=False):
    asset = unreal.load_asset(path)
    if asset is not None:
        library.save_loaded_asset(asset, False)
log("Katana import complete")
