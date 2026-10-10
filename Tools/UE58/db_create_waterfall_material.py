# Run inside the Unreal Editor Python environment (UE 5.8):
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_create_waterfall_material.py
#
# Falling water (M_DB_Waterfall, Phase 17): lit translucent (forward-shaded surface, so sun, sky and Lumen light it like
# the sea), two-sided. Two layers of the engine's tiling noise run down the sheet at different speeds and scales; where
# they meet the water turns to white foam and thickens, between them it thins to glassy streaks. Everything is a
# parameter so ADBWaterfall can tune sheet and splash: WaterColor, FoamColor, FlowSpeed, Streaks (UV tiling across),
# Opacity, FoamAmount.
import unreal

FOLDER = "/Game/DarkBlood/Art/Materials/Master"
NAME = "M_DB_Waterfall"
NOISE = "/Engine/EngineMaterials/Good64x64TilingNoiseHighFreq.Good64x64TilingNoiseHighFreq"

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
E = unreal


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


def vector(material, name, value, x, y):
    return node(material, E.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=value)


# The foam flow texture of the Paragon packs (Fab Standard license, local like Content/Paragon*/): copied next to the
# material, also git-ignored. Without the Paragon pack the copy is missing and the material falls back to engine noise
# when this script runs; an existing material then shows the default texture until the script runs again.
TEXTURE_FOLDER = "/Game/DarkBlood/Art/Textures/Water"
FOAM_SOURCE = "/Game/ParagonGrux/FX/Textures/Tile/Organic/T_WaterFlow_01_Foam_Tiled_4Real"
FOAM = TEXTURE_FOLDER + "/T_DB_WaterfallFoam"
if not library.does_asset_exist(FOAM):
    if library.does_asset_exist(FOAM_SOURCE):
        library.duplicate_asset(FOAM_SOURCE, FOAM)
        library.save_asset(FOAM, False)
    else:
        unreal.log_warning("DBWATERFALL foam texture missing, using engine noise")
foam_texture = unreal.load_asset(FOAM) if library.does_asset_exist(FOAM) else unreal.load_asset(NOISE)

path = FOLDER + "/" + NAME
if library.does_asset_exist(path):
    library.delete_asset(path)
material = tools.create_asset(NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
material.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
material.set_editor_property("two_sided", True)

flow = scalar(material, "FlowSpeed", 1.0, -1700, 200)
streaks = scalar(material, "Streaks", 3.0, -1700, 60)
down_tiles = scalar(material, "DownTiles", 6.0, -1700, 340)


def layer(y, scale_across, scale_down, speed, seed_offset):
    """Foam running down the sheet: UV * (Streaks * across, DownTiles * down) + (seed, -time * FlowSpeed * speed)."""
    uv = node(material, E.MaterialExpressionTextureCoordinate, -1500, y)
    across = node(material, E.MaterialExpressionMultiply, -1350, y - 60, const_b=scale_across)
    link(streaks, "", across, "A")
    down = node(material, E.MaterialExpressionMultiply, -1350, y + 40, const_b=scale_down)
    link(down_tiles, "", down, "A")
    tiling = node(material, E.MaterialExpressionAppendVector, -1200, y - 60)
    link(across, "", tiling, "A")
    link(down, "", tiling, "B")
    scaled = binary(material, E.MaterialExpressionMultiply, uv, tiling, -1050, y)
    time = node(material, E.MaterialExpressionTime, -1350, y + 140)
    rate = node(material, E.MaterialExpressionMultiply, -1200, y + 140, const_b=-speed)
    link(flow, "", rate, "A")
    moved = binary(material, E.MaterialExpressionMultiply, time, rate, -1050, y + 140)
    offset = node(material, E.MaterialExpressionAppendVector, -900, y + 140)
    seed = node(material, E.MaterialExpressionConstant, -1050, y + 240, r=seed_offset)
    link(seed, "", offset, "A")
    link(moved, "", offset, "B")
    coords = binary(material, E.MaterialExpressionAdd, scaled, offset, -750, y)
    sample = node(material, E.MaterialExpressionTextureSample, -600, y, texture=foam_texture)
    link(coords, "", sample, "UVs")
    return sample


fast = layer(-200, 1.0, 1.0, 1.0, 0.0)
slow = layer(300, 1.6, 0.6, 1.55, 0.37)
mixed = binary(material, E.MaterialExpressionMultiply, fast, slow, -380, 100, "R", "R")
boosted = node(material, E.MaterialExpressionMultiply, -240, 100, const_b=2.2)
link(mixed, "", boosted, "A")
foam_amount = scalar(material, "FoamAmount", 1.0, -380, 260)
foam_raw = binary(material, E.MaterialExpressionMultiply, boosted, foam_amount, -100, 160)
foam = node(material, E.MaterialExpressionClamp, 40, 160, min_default=0.0, max_default=1.0)
link(foam_raw, "", foam, "")

water = vector(material, "WaterColor", unreal.LinearColor(0.32, 0.42, 0.46, 1.0), -100, -260)
white = vector(material, "FoamColor", unreal.LinearColor(0.86, 0.9, 0.92, 1.0), -100, -100)
base = node(material, E.MaterialExpressionLinearInterpolate, 200, -180)
link(water, "", base, "A")
link(white, "", base, "B")
link(foam, "", base, "Alpha")
mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)

rough = node(material, E.MaterialExpressionLinearInterpolate, 200, 0, const_a=0.08, const_b=0.6)
link(foam, "", rough, "Alpha")
mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

# A little light of its own in the foam, so the fall still reads at dusk.
glow = node(material, E.MaterialExpressionMultiply, 200, 120, const_b=0.06)
link(foam, "", glow, "A")
glow_color = binary(material, E.MaterialExpressionMultiply, white, glow, 360, 120)
mel.connect_material_property(glow_color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

opacity_param = scalar(material, "Opacity", 0.55, 40, 320)
thick = node(material, E.MaterialExpressionMultiply, 200, 320, const_b=0.45)
link(foam, "", thick, "A")
opacity = binary(material, E.MaterialExpressionAdd, opacity_param, thick, 360, 320)

# Soft sides: the sheet thins out towards its edges (sin(pi * u) ^ EdgeSoftness), so it never reads as a rectangle.
edge_uv = node(material, E.MaterialExpressionTextureCoordinate, 40, 480)
edge_u = node(material, E.MaterialExpressionComponentMask, 200, 480, r=True, g=False, b=False, a=False)
link(edge_uv, "", edge_u, "")
edge_angle = node(material, E.MaterialExpressionMultiply, 340, 480, const_b=3.14159)
link(edge_u, "", edge_angle, "A")
edge_sine = node(material, E.MaterialExpressionSine, 480, 480, period=6.28318)
link(edge_angle, "", edge_sine, "")
edge_positive = node(material, E.MaterialExpressionMax, 620, 480, const_b=0.0)
link(edge_sine, "", edge_positive, "A")
softness = scalar(material, "EdgeSoftness", 0.7, 620, 600)
edge_fade = node(material, E.MaterialExpressionPower, 760, 480)
link(edge_positive, "", edge_fade, "Base")
link(softness, "", edge_fade, "Exp")
faded = binary(material, E.MaterialExpressionMultiply, opacity, edge_fade, 900, 360)
opacity_clamped = node(material, E.MaterialExpressionClamp, 1060, 360, min_default=0.0, max_default=0.95)
link(faded, "", opacity_clamped, "")
mel.connect_material_property(opacity_clamped, "", unreal.MaterialProperty.MP_OPACITY)

mel.recompile_material(material)
library.save_loaded_asset(material, False)
unreal.log("DBWATERFALL created " + path)
