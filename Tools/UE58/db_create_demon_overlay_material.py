# Run inside the Unreal Editor Python environment (UE 5.8):
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_create_demon_overlay_material.py
#
# The demonic mark (M_DB_DemonOverlay): an overlay material laid over a borrowed body (Paragon) so it reads in the
# colour of its vassal - a dark colour cast over the whole body and a slowly pulsing rim glow. Unlit, translucent,
# one Fresnel and one sine, no textures: one cheap extra pass per character, culled with the overlay draw distance.
# Parameters: AccentColor, BodyOpacity, BodyGlow, RimIntensity, RimOpacity, Pulse.
import unreal

FOLDER = "/Game/DarkBlood/Art/Materials/Master"
NAME = "M_DB_DemonOverlay"

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


def binary(material, cls, a, b, x, y):
    expression = node(material, cls, x, y)
    link(a, "", expression, "A")
    link(b, "", expression, "B")
    return expression


def scalar(material, name, value, x, y):
    return node(material, E.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


path = FOLDER + "/" + NAME
material = unreal.load_asset(path)
if material:
    mel.delete_all_material_expressions(material)
else:
    material = tools.create_asset(NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("used_with_skeletal_mesh", True)

# Rim: Fresnel^5 (a thin edge), pulsing between 0.8 and 1.2 of its strength.
fresnel = node(material, E.MaterialExpressionFresnel, -900, 0, exponent=5.0, base_reflect_fraction=0.0)
time = node(material, E.MaterialExpressionTime, -1100, 200)
speed = scalar(material, "Pulse", 1.4, -1100, 300)
phase = binary(material, E.MaterialExpressionMultiply, time, speed, -950, 240)
wave = node(material, E.MaterialExpressionSine, -800, 240, period=6.283)
link(phase, "", wave, "")
swing = node(material, E.MaterialExpressionMultiply, -660, 240, const_b=0.2)
link(wave, "", swing, "A")
pulse = node(material, E.MaterialExpressionAdd, -520, 240, const_b=1.0)
link(swing, "", pulse, "A")
rim = binary(material, E.MaterialExpressionMultiply, fresnel, pulse, -380, 100)

color = node(material, E.MaterialExpressionVectorParameter, -380, -260, parameter_name="AccentColor",
             default_value=unreal.LinearColor(1.0, 0.06, 0.04, 1.0))
body_glow = scalar(material, "BodyGlow", 0.03, -380, -120)
rim_intensity = scalar(material, "RimIntensity", 1.5, -380, 0)
rim_glow = binary(material, E.MaterialExpressionMultiply, rim, rim_intensity, -200, 40)
glow = binary(material, E.MaterialExpressionAdd, body_glow, rim_glow, -40, -40)
emissive = binary(material, E.MaterialExpressionMultiply, color, glow, 120, -120)
mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

body_opacity = scalar(material, "BodyOpacity", 0.2, -200, 260)
rim_opacity = scalar(material, "RimOpacity", 0.5, -380, 380)
rim_cover = binary(material, E.MaterialExpressionMultiply, rim, rim_opacity, -200, 380)
cover = binary(material, E.MaterialExpressionAdd, body_opacity, rim_cover, -40, 300)
clamped = node(material, E.MaterialExpressionClamp, 120, 300, min_default=0.0, max_default=0.9)
link(cover, "", clamped, "")
mel.connect_material_property(clamped, "", unreal.MaterialProperty.MP_OPACITY)

mel.recompile_material(material)
library.save_loaded_asset(material, False)
unreal.log("DBOVERLAY created " + path)
