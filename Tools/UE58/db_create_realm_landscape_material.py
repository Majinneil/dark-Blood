# Run inside the Unreal Editor Python environment (UE 5.8) after polyhaven_fetch.py:
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_create_realm_landscape_material.py
#
# Landscape material of the open world (L_Realm): 8 weight-blended paint layers with Poly Haven CC0 textures, world-
# projected, shared samplers (no sampler limit), two-scale macro variation against tiling, glowing lava cracks and a
# faint pulse in corrupted soil. Layer names must match DBBuildRealmCommandlet (Meadow, Forest, Rock, Snow, Sand, Soil,
# Corrupt, Lava).
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "SourceArt", "PolyHaven", "Textures")
TEXTURE_ROOT = "/Game/DarkBlood/Art/Textures/PolyHaven"
FOLDER = "/Game/DarkBlood/Art/Materials/Landscape"

# layer, texture set, tile size (cm), tint, roughness
LAYERS = [
    ("Meadow", "leafy_grass", 420.0, (0.45, 0.72, 0.3), 0.85),
    ("Forest", "forest_ground_04", 380.0, (0.85, 0.85, 0.8), 0.9),
    ("Rock", "cliff_side", 1300.0, (0.62, 0.62, 0.64), 0.75),
    ("Snow", "snow_02", 600.0, (0.86, 0.88, 0.94), 0.55),
    ("Sand", "aerial_sand", 1200.0, (1.0, 0.92, 0.8), 0.9),
    ("Soil", "rocky_trail_02", 420.0, (0.85, 0.8, 0.75), 0.85),
    ("Corrupt", "burned_ground_01", 500.0, (0.4, 0.22, 0.22), 0.8),
    ("Lava", "cracked_red_ground", 600.0, (0.42, 0.28, 0.26), 0.7),
]

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
E = unreal
SHARED = unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS


def log(message):
    unreal.log("DBREALMMAT " + message)


def import_texture(asset, kind):
    path = "%s/%s/T_%s_%s" % (TEXTURE_ROOT, asset, asset, kind)
    texture = unreal.load_asset(path)
    if texture is not None:
        return texture
    short = {"D": "diff", "N": "nor_gl", "ARM": "arm"}[kind]
    folder = os.path.join(SOURCE, asset)
    files = [f for f in os.listdir(folder) if "_%s_" % short in f] if os.path.isdir(folder) else []
    if not files:
        raise RuntimeError("missing source texture %s %s (run polyhaven_fetch.py)" % (asset, kind))
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(folder, files[0]))
    task.set_editor_property("destination_path", TEXTURE_ROOT + "/" + asset)
    task.set_editor_property("destination_name", "T_%s_%s" % (asset, kind))
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    tools.import_asset_tasks([task])
    texture = unreal.load_asset(path)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    if kind == "N":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("srgb", False)
        texture.set_editor_property("flip_green_channel", True)
    library.save_loaded_asset(texture, False)
    log("imported texture %s %s" % (asset, kind))
    return texture


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


def constant(material, value, x, y):
    return node(material, E.MaterialExpressionConstant, x, y, r=value)


def vector_rgb(material, name, value, x, y):
    parameter = node(material, E.MaterialExpressionVectorParameter, x, y, parameter_name=name,
                     default_value=unreal.LinearColor(value[0], value[1], value[2], 1.0))
    mask = node(material, E.MaterialExpressionComponentMask, x + 220, y, r=True, g=True, b=True, a=False)
    link(parameter, "", mask, "")
    return mask


path = FOLDER + "/M_DB_Realm_Landscape"
material = unreal.load_asset(path) or tools.create_asset("M_DB_Realm_Landscape", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
mel.delete_all_material_expressions(material)

world = node(material, E.MaterialExpressionWorldPosition, -3200, 0)
world_xy = node(material, E.MaterialExpressionComponentMask, -3000, 0, r=True, g=True, b=False, a=False)
link(world, "", world_xy, "")

# Macro variation: two scales of the engine noise break up the tiling over kilometers.
macro_texture = unreal.load_asset("/Engine/EngineMaterials/T_Default_MacroVariation")
macro_a = node(material, E.MaterialExpressionTextureSample, -2600, -600, texture=macro_texture, sampler_source=SHARED)
link(binary(material, E.MaterialExpressionDivide, world_xy, constant(material, 4000.0, -2900, -560), -2800, -600), "", macro_a, "UVs")
macro_b = node(material, E.MaterialExpressionTextureSample, -2600, -350, texture=macro_texture, sampler_source=SHARED)
link(binary(material, E.MaterialExpressionDivide, world_xy, constant(material, 31000.0, -2900, -310), -2800, -350), "", macro_b, "UVs")
macro = binary(material, E.MaterialExpressionMultiply, macro_a, macro_b, -2300, -500, "R", "G")
macro_factor = node(material, E.MaterialExpressionLinearInterpolate, -2100, -500)
link(constant(material, 0.72, -2300, -420), "", macro_factor, "A")
link(constant(material, 1.18, -2300, -380), "", macro_factor, "B")
macro_sat = node(material, E.MaterialExpressionSaturate, -2200, -460)
link(binary(material, E.MaterialExpressionMultiply, macro, constant(material, 2.5, -2350, -460), -2250, -460), "", macro_sat, "")
link(macro_sat, "", macro_factor, "Alpha")

color_blend = node(material, E.MaterialExpressionLandscapeLayerBlend, -600, -200)
normal_blend = node(material, E.MaterialExpressionLandscapeLayerBlend, -600, 300)
rough_blend = node(material, E.MaterialExpressionLandscapeLayerBlend, -600, 700)
for blend in (color_blend, normal_blend, rough_blend):
    inputs = []
    for index, (name, _, _, _, _) in enumerate(LAYERS):
        layer = unreal.LayerBlendInput()
        layer.set_editor_property("layer_name", name)
        layer.set_editor_property("blend_type", unreal.LandscapeLayerBlendType.LB_WEIGHT_BLEND)
        layer.set_editor_property("preview_weight", 1.0 if index == 0 else 0.0)
        inputs.append(layer)
    blend.set_editor_property("layers", inputs)

lava_diffuse = None
for index, (name, asset, tile, tint, rough) in enumerate(LAYERS):
    y = -1600 + index * 420
    uv = binary(material, E.MaterialExpressionDivide, world_xy, constant(material, tile, -2600, y + 60), -2400, y)
    diffuse = node(material, E.MaterialExpressionTextureSample, -2100, y, texture=import_texture(asset, "D"), sampler_source=SHARED)
    link(uv, "", diffuse, "UVs")
    normal = node(material, E.MaterialExpressionTextureSample, -2100, y + 200, texture=import_texture(asset, "N"),
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, sampler_source=SHARED)
    link(uv, "", normal, "UVs")
    tinted = binary(material, E.MaterialExpressionMultiply, diffuse, vector_rgb(material, name + "_Tint", tint, -1900, y + 100), -1500, y, "RGB")
    link(binary(material, E.MaterialExpressionMultiply, tinted, macro_factor, -1300, y), "", color_blend, "Layer " + name)
    link(normal, "RGB", normal_blend, "Layer " + name)
    link(node(material, E.MaterialExpressionScalarParameter, -1300, y + 200, parameter_name=name + "_Roughness", default_value=rough), "",
         rough_blend, "Layer " + name)
    if name == "Lava":
        lava_diffuse = diffuse

# Emissive: lava glows in its red cracks, corrupted soil pulses faintly.
lava_weight = node(material, E.MaterialExpressionLandscapeLayerSample, -1200, 1300, parameter_name="Lava")
corrupt_weight = node(material, E.MaterialExpressionLandscapeLayerSample, -1200, 1500, parameter_name="Corrupt")
crack = node(material, E.MaterialExpressionSaturate, -1000, 1200)
link(binary(material, E.MaterialExpressionMultiply, binary(material, E.MaterialExpressionSubtract, lava_diffuse, lava_diffuse, -1300, 1150, "R", "G"),
            constant(material, 5.0, -1300, 1200), -1150, 1200), "", crack, "")
glow = vector_rgb(material, "LavaGlow", (1.0, 0.18, 0.03), -1200, 1000)
lava_emissive = binary(material, E.MaterialExpressionMultiply, binary(material, E.MaterialExpressionMultiply, glow, crack, -900, 1100),
                       binary(material, E.MaterialExpressionMultiply, lava_weight,
                              node(material, E.MaterialExpressionScalarParameter, -1000, 1400, parameter_name="LavaStrength", default_value=10.0),
                              -900, 1300), -700, 1200)
time = node(material, E.MaterialExpressionTime, -1300, 1650)
sine = node(material, E.MaterialExpressionSine, -1100, 1650, period=6.283)
link(binary(material, E.MaterialExpressionMultiply, time, constant(material, 1.3, -1300, 1700), -1200, 1650), "", sine, "")
pulse = binary(material, E.MaterialExpressionAdd, binary(material, E.MaterialExpressionMultiply, sine, constant(material, 0.4, -1100, 1720), -1000, 1650),
               constant(material, 0.6, -1000, 1720), -900, 1650)
corrupt_emissive = binary(material, E.MaterialExpressionMultiply,
                          binary(material, E.MaterialExpressionMultiply, vector_rgb(material, "CorruptGlow", (0.06, 0.004, 0.002), -1200, 1800), corrupt_weight,
                                 -900, 1800), pulse, -700, 1750)
emissive = binary(material, E.MaterialExpressionAdd, lava_emissive, corrupt_emissive, -450, 1400)

# ---- Grass: the engine plants these near the camera from the layer weights (no placement data in the map) ----------
POLYHAVEN = "/Game/DarkBlood/Art/Environment/PolyHaven/"
GRASS_ROOT = "/Game/DarkBlood/Art/Environment/Grass"


def meshes(model, names):
    return [unreal.load_asset("%s%s/%s_1k/StaticMeshes/%s" % (POLYHAVEN, model, model, name)) for name in names]


# grass type: layer, [(meshes, density per 10 m2, end cull cm, scale range, shadows)]
GRASS = {
    "Meadow": [(meshes("grass_medium_01", ["grass_medium_01_large_a_LOD0", "grass_medium_01_large_b_LOD0", "grass_medium_01_mid_a_LOD0",
                                           "grass_medium_01_tall_a_LOD0", "grass_medium_01_tall_b_LOD0"]), 28.0, 6500, (1.6, 2.8), False),
               (meshes("grass_medium_02", ["grass_medium_02_c", "grass_medium_02_d", "grass_medium_02_e"]), 10.0, 6500, (1.8, 3.0), False),
               (meshes("fern_02", ["fern_02_a", "fern_02_b"]), 0.4, 12000, (0.8, 1.3), True)],
    "Forest": [(meshes("fern_02", ["fern_02_a", "fern_02_b", "fern_02_c", "fern_02_d"]), 3.0, 12000, (0.9, 1.5), True),
               (meshes("shrub_02", ["shrub_02_a", "shrub_02_b", "shrub_02_c"]), 0.8, 16000, (0.8, 1.4), True),
               (meshes("grass_medium_01", ["grass_medium_01_mid_a_LOD0", "grass_medium_01_mid_b_LOD0"]), 8.0, 5000, (1.5, 2.4), False)],
    "Soil": [(meshes("grass_medium_01", ["grass_medium_01_small_a_LOD0", "grass_medium_01_tiny_a_LOD0", "grass_medium_01_mid_a_LOD0"]), 6.0, 5000,
              (1.4, 2.4), False)],
    "Rock": [(meshes("rock_moss_set_01", ["rock_moss_set_01_rock01", "rock_moss_set_01_rock02", "rock_moss_set_01_rock03"]), 0.25, 20000, (0.6, 1.8),
              True)],
}
grass_types = {}
for layer, varieties in GRASS.items():
    name = "GT_DB_Realm_" + layer
    grass = unreal.load_asset(GRASS_ROOT + "/" + name) or tools.create_asset(name, GRASS_ROOT, unreal.LandscapeGrassType, None)
    entries = []
    for mesh_list, density, cull, scale, shadows in varieties:
        for mesh in mesh_list:
            if mesh is None:
                continue
            variety = unreal.GrassVariety()
            variety.set_editor_property("grass_mesh", mesh)
            per_mesh = unreal.PerPlatformFloat()
            per_mesh.set_editor_property("default", density / len(mesh_list))
            variety.set_editor_property("grass_density", per_mesh)
            start = unreal.PerPlatformInt()
            start.set_editor_property("default", int(cull * 0.6))
            end = unreal.PerPlatformInt()
            end.set_editor_property("default", int(cull))
            variety.set_editor_property("start_cull_distance", start)
            variety.set_editor_property("end_cull_distance", end)
            variety.set_editor_property("scaling", unreal.GrassScaling.UNIFORM)
            variety.set_editor_property("scale_x", unreal.FloatInterval(scale[0], scale[1]))
            variety.set_editor_property("random_rotation", True)
            variety.set_editor_property("align_to_surface", True)
            variety.set_editor_property("cast_dynamic_shadow", shadows)
            entries.append(variety)
    grass.set_editor_property("grass_varieties", entries)
    library.save_loaded_asset(grass, False)
    grass_types[layer] = grass
    log("grass type %s: %d varieties" % (name, len(entries)))

grass_output = node(material, E.MaterialExpressionLandscapeGrassOutput, -200, 2000)
inputs = []
for layer, grass in grass_types.items():
    grass_input = unreal.GrassInput()
    grass_input.set_editor_property("name", layer)
    grass_input.set_editor_property("grass_type", grass)
    inputs.append(grass_input)
grass_output.set_editor_property("grass_types", inputs)
for index, layer in enumerate(grass_types):
    sample = node(material, E.MaterialExpressionLandscapeLayerSample, -500, 2000 + index * 120, parameter_name=layer)
    link(sample, "", grass_output, layer)

P = unreal.MaterialProperty
mel.connect_material_property(color_blend, "", P.MP_BASE_COLOR)
mel.connect_material_property(normal_blend, "", P.MP_NORMAL)
mel.connect_material_property(rough_blend, "", P.MP_ROUGHNESS)
mel.connect_material_property(emissive, "", P.MP_EMISSIVE_COLOR)
mel.recompile_material(material)
library.save_loaded_asset(material, False)
log("M_DB_Realm_Landscape compiled (%d layers)" % len(LAYERS))
