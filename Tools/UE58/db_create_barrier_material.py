# Run inside the Unreal Editor Python environment (UE 5.8):
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_create_barrier_material.py
#
# The blood barrier of the boss arenas (M_DB_BloodBarrier): unlit, translucent, two-sided; its glow runs in slow bands
# along the wall (one sine of world position and time, no textures, no noise node: cheap even when it fills the screen).
# Color and strength are parameters (BarrierColor, Intensity, Opacity) so every arena tints it in its boss' color.
import unreal

FOLDER = "/Game/DarkBlood/Art/Materials/Master"
NAME = "M_DB_BloodBarrier"

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


path = FOLDER + "/" + NAME
if library.does_asset_exist(path):
    library.delete_asset(path)
material = tools.create_asset(NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("two_sided", True)

# Bands: sin((world.x + world.y) * 0.004 + time * 1.6) * 0.5 + 0.5
world = node(material, E.MaterialExpressionWorldPosition, -1400, 0)
wx = node(material, E.MaterialExpressionComponentMask, -1200, -60, r=True, g=False, b=False, a=False)
wy = node(material, E.MaterialExpressionComponentMask, -1200, 60, r=False, g=True, b=False, a=False)
link(world, "", wx, "")
link(world, "", wy, "")
along = binary(material, E.MaterialExpressionAdd, wx, wy, -1000, 0)
scaled = node(material, E.MaterialExpressionMultiply, -850, 0, const_b=0.004)
link(along, "", scaled, "A")
time = node(material, E.MaterialExpressionTime, -1000, 160)
drift = node(material, E.MaterialExpressionMultiply, -850, 160, const_b=1.6)
link(time, "", drift, "A")
phase = binary(material, E.MaterialExpressionAdd, scaled, drift, -700, 60)
wave = node(material, E.MaterialExpressionSine, -560, 60, period=6.283)
link(phase, "", wave, "")
half = node(material, E.MaterialExpressionMultiply, -420, 60, const_b=0.5)
link(wave, "", half, "A")
band = node(material, E.MaterialExpressionAdd, -280, 60, const_b=0.5)
link(half, "", band, "A")

# Height fade: brighter at the foot of the wall (V of the cube faces).
uv = node(material, E.MaterialExpressionTextureCoordinate, -700, 300)
v = node(material, E.MaterialExpressionComponentMask, -520, 300, r=False, g=True, b=False, a=False)
link(uv, "", v, "")

color = node(material, E.MaterialExpressionVectorParameter, -420, -200, parameter_name="BarrierColor",
             default_value=unreal.LinearColor(1.0, 0.06, 0.04, 1.0))
intensity = scalar(material, "Intensity", 3.0, -420, -60)
glow = binary(material, E.MaterialExpressionAdd, band, v, -140, 120)
tinted = binary(material, E.MaterialExpressionMultiply, color, glow, 40, -60)
emissive = binary(material, E.MaterialExpressionMultiply, tinted, intensity, 220, -60)
mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

opacity_base = scalar(material, "Opacity", 0.22, -140, 300)
opacity_mix = node(material, E.MaterialExpressionMultiply, 40, 300, const_b=0.35)
link(glow, "", opacity_mix, "A")
opacity = binary(material, E.MaterialExpressionAdd, opacity_base, opacity_mix, 220, 260)
clamped = node(material, E.MaterialExpressionClamp, 380, 260, min_default=0.0, max_default=0.85)
link(opacity, "", clamped, "")
mel.connect_material_property(clamped, "", unreal.MaterialProperty.MP_OPACITY)

mel.recompile_material(material)
library.save_loaded_asset(material, False)
unreal.log("DBBARRIER created " + path)
