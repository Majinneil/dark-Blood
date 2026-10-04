# Run inside the Unreal Editor Python environment (UE 5.8) after db_create_sea_material.py, then rebuild the realm:
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_create_river_materials.py
#   UnrealEditor-Cmd DarkBlood.uproject -run=DBBuildRealm
#
# Water of the highland rivers in L_Realm (DBBuildRealmCommandlet, step 5):
# - MI_DB_Water_River: the sea material (M_DB_Sea) with greener, lighter water and a calmer chop for the river surfaces.
# - M_DB_Waterfall / MI_DB_Waterfall: foaming water that runs down the steep waterfall surfaces. Its pattern is projected
#   from the world position (across: X+Y, down: Z), so it streams downwards on any plane whatever its UVs; two layers of
#   the sea's wave normal map at different sizes and speeds give the foam streaks. Opaque, like the sea, to stay cheap.
# Both base materials get the "Instanced Static Meshes" usage: the rivers are instanced per river.
import unreal

TEXTURE = "/Game/DarkBlood/Art/Textures/Generated/T_DB_Sea_N"
MATERIAL_FOLDER = "/Game/DarkBlood/Art/Materials/Master"
INSTANCE_FOLDER = "/Game/DarkBlood/Art/Materials/Instances"
SEA = MATERIAL_FOLDER + "/M_DB_Sea"

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
E = unreal


def log(message):
    unreal.log("DBRIVER " + message)


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


def scalar(material, name, value, x, y):
    return node(material, E.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def color(material, name, value, x, y):
    parameter = node(material, E.MaterialExpressionVectorParameter, x, y, parameter_name=name,
                     default_value=unreal.LinearColor(value[0], value[1], value[2], 1.0))
    mask = node(material, E.MaterialExpressionComponentMask, x + 220, y, r=True, g=True, b=True, a=False)
    link(parameter, "", mask, "")
    return mask


def instance(name, parent, vectors, scalars):
    path = INSTANCE_FOLDER + "/" + name
    if library.does_asset_exist(path):
        library.delete_asset(path)
    asset = tools.create_asset(name, INSTANCE_FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(asset, parent)
    for key, value in vectors.items():
        mel.set_material_instance_vector_parameter_value(asset, key, unreal.LinearColor(value[0], value[1], value[2], 1.0))
    for key, value in scalars.items():
        mel.set_material_instance_scalar_parameter_value(asset, key, value)
    mel.update_material_instance(asset)
    library.save_loaded_asset(asset, False)
    log("instance %s -> %s" % (name, parent.get_name()))


texture = unreal.load_asset(TEXTURE)
sea = unreal.load_asset(SEA)
if texture is None or sea is None:
    raise RuntimeError("missing %s or %s (run Tools/UE58/db_create_sea_material.py)" % (TEXTURE, SEA))

# ---- 1. River surfaces: the sea material, river-coloured ----------------------------------------------------------------
if not sea.get_editor_property("used_with_instanced_static_meshes"):
    sea.set_editor_property("used_with_instanced_static_meshes", True)
    mel.recompile_material(sea)
    library.save_loaded_asset(sea, False)
    log("M_DB_Sea: instanced static mesh usage")
instance("MI_DB_Water_River", sea, {"DeepColor": (0.010, 0.034, 0.030), "CrestColor": (0.030, 0.085, 0.070)},
         {"WaveStrength": 0.35, "Roughness": 0.08})

# ---- 2. Waterfall master ------------------------------------------------------------------------------------------------
path = MATERIAL_FOLDER + "/M_DB_Waterfall"
if library.does_asset_exist(path):
    library.delete_asset(path)
material = tools.create_asset("M_DB_Waterfall", MATERIAL_FOLDER, unreal.Material, unreal.MaterialFactoryNew())
material.set_editor_property("used_with_static_lighting", False)
material.set_editor_property("used_with_instanced_static_meshes", True)

position = node(material, E.MaterialExpressionWorldPosition, -2400, 0)
across_mask = node(material, E.MaterialExpressionComponentMask, -2200, -100, r=True, g=True, b=False, a=False)
link(position, "", across_mask, "")
# Across the fall: X + Y (any orientation of the plane gets a horizontal coordinate that changes along its width).
across = node(material, E.MaterialExpressionDotProduct, -2000, -100)
link(across_mask, "", across, "A")
link(node(material, E.MaterialExpressionConstant2Vector, -2200, 0, r=0.7071, g=0.7071), "", across, "B")
height = node(material, E.MaterialExpressionComponentMask, -2200, 150, r=False, g=False, b=True, a=False)
link(position, "", height, "")
time = node(material, E.MaterialExpressionTime, -2200, 350)
speed_scale = scalar(material, "FallSpeed", 1.0, -2200, 450)

# (tile cm across, tile cm down, fall speed cm/s): broad sheets and fine streaks
LAYERS = [(900.0, 1400.0, 650.0), (320.0, 520.0, 1000.0)]
samples = []
for index, (tile_u, tile_v, fall) in enumerate(LAYERS):
    y = index * 320
    u = binary(material, E.MaterialExpressionDivide, across, constant(material, tile_u, -1800, -200 + y), -1600, -100 + y)
    moved = binary(material, E.MaterialExpressionMultiply, time,
                   binary(material, E.MaterialExpressionMultiply, speed_scale, constant(material, fall, -2000, 520 + y), -1800, 470 + y),
                   -1600, 350 + y)
    # The pattern moves down: a fixed V lies lower every second.
    down = binary(material, E.MaterialExpressionAdd, height, moved, -1400, 150 + y)
    v = binary(material, E.MaterialExpressionDivide, down, constant(material, tile_v, -1400, 300 + y), -1200, 150 + y)
    uv = binary(material, E.MaterialExpressionAppendVector, u, v, -1000, y)
    sample = node(material, E.MaterialExpressionTextureSample, -800, y, texture=texture,
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    link(uv, "", sample, "UVs")
    samples.append(sample)

# Foam: where either layer's wave slope is strong (decoded normal X/Y far from 0).
slope_a = node(material, E.MaterialExpressionAbs, -550, -100)
link(samples[0], "R", slope_a, "")
slope_b = node(material, E.MaterialExpressionAbs, -550, 250)
link(samples[1], "G", slope_b, "")
streaks = binary(material, E.MaterialExpressionAdd, slope_a, slope_b, -350, 50)
foam_amount = scalar(material, "FoamAmount", 2.2, -550, 450)
foam_raw = binary(material, E.MaterialExpressionMultiply, streaks, foam_amount, -150, 50)
foam_bias = binary(material, E.MaterialExpressionAdd, foam_raw, constant(material, 0.2, -150, 200), 0, 50)
foam = node(material, E.MaterialExpressionSaturate, 150, 50)
link(foam_bias, "", foam, "")

water = color(material, "WaterColor", (0.035, 0.085, 0.080), -150, -450)
spray = color(material, "FoamColor", (0.78, 0.84, 0.84), -150, -300)
base = node(material, E.MaterialExpressionLinearInterpolate, 350, -350)
link(water, "", base, "A")
link(spray, "", base, "B")
link(foam, "", base, "Alpha")
mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)

# Normal: the two layers combined (X/Y summed, Z = 1, normalized).
normal_a = node(material, E.MaterialExpressionComponentMask, -550, 600, r=True, g=True, b=False, a=False)
link(samples[0], "RGB", normal_a, "")
normal_b = node(material, E.MaterialExpressionComponentMask, -550, 700, r=True, g=True, b=False, a=False)
link(samples[1], "RGB", normal_b, "")
normal_xy = binary(material, E.MaterialExpressionAdd, normal_a, normal_b, -350, 650)
normal = binary(material, E.MaterialExpressionAppendVector, normal_xy, constant(material, 1.0, -350, 800), -150, 650)
normalized = node(material, E.MaterialExpressionNormalize, 50, 650)
link(normal, "", normalized, "")
mel.connect_material_property(normalized, "", unreal.MaterialProperty.MP_NORMAL)

# Foam is rough, the clear water between the streaks glossy.
roughness = node(material, E.MaterialExpressionLinearInterpolate, 350, 300)
link(constant(material, 0.08, 150, 250), "", roughness, "A")
link(constant(material, 0.55, 150, 330), "", roughness, "B")
link(foam, "", roughness, "Alpha")
mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

mel.recompile_material(material)
library.save_loaded_asset(material, False)
log("material M_DB_Waterfall")

instance("MI_DB_Waterfall", material, {}, {})
log("done - rebuild the realm (-run=DBBuildRealm) to put the water into L_Realm")
