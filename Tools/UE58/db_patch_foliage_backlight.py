# Run inside the Unreal Editor Python environment (UE 5.8):
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_patch_foliage_backlight.py
#
# Backlight for leaves and blossoms (M_DB_Foliage_Master, Two Sided Foliage shading): when the camera looks through the
# foliage towards the sun, the transmitted light grows (DBBacklight) and the leaf edges glow in the sun's current colour
# (DBBacklightGlow - red at dusk, read from the sky atmosphere, so it follows the time of day without any blueprint).
# Both are small HLSL custom nodes appended to the existing graph; re-running replaces them. Parameters for the
# instances (cherry blossoms, leaves): BacklightPower (tightness around the sun), BacklightBoost, BacklightGlow.
import unreal

MASTER = "/Game/DarkBlood/Art/Materials/Master/M_DB_Foliage_Master"
mel = unreal.MaterialEditingLibrary
E = unreal

TRANSMIT = """
// View-dependent transmission: looking from the camera through the leaf towards the sun.
float Back = saturate(dot(-CameraVector, SunDirection));
Back = pow(Back, Power);
return Subsurface * (1.0 + Boost * Back);
"""

GLOW = """
// Rim glow of backlit leaves in the colour and strength of the atmosphere-filtered sun (red at dusk, none at night).
float Back = saturate(dot(-CameraVector, SunDirection));
Back = pow(Back, Power * 0.5);
float Horizon = saturate(1.0 - abs(SunDirection.z) * 2.0); // strongest when the sun is low
return Subsurface * Illuminance * Back * Glow * (0.35 + Horizon);
"""


def node(material, cls, x, y, **props):
    expression = mel.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def link(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError("link failed: %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def custom(material, x, y, description, code, names):
    expression = node(material, E.MaterialExpressionCustom, x, y, code=code, description=description,
                      output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for name in names:
        item = unreal.CustomInput()
        item.set_editor_property("input_name", name)
        inputs.append(item)
    expression.set_editor_property("inputs", inputs)
    return expression


material = unreal.load_asset(MASTER)
# Remove an earlier patch (and remember what fed it, so the original graph is restored first).
subsurface_source = mel.get_material_property_input_node(material, unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
emissive_source = mel.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
if subsurface_source and subsurface_source.get_editor_property("description") == "DBBacklight" if isinstance(subsurface_source, E.MaterialExpressionCustom) else False:
    raise SystemExit("already patched - delete the DBBacklight nodes in the material editor to re-patch")

power = node(material, E.MaterialExpressionScalarParameter, -900, 1400, parameter_name="BacklightPower", default_value=6.0, group="03 Surface")
boost = node(material, E.MaterialExpressionScalarParameter, -900, 1500, parameter_name="BacklightBoost", default_value=2.5, group="03 Surface")
glow = node(material, E.MaterialExpressionScalarParameter, -900, 1600, parameter_name="BacklightGlow", default_value=0.06, group="03 Surface")
camera = node(material, E.MaterialExpressionCameraVectorWS, -900, 1700)
sun = node(material, E.MaterialExpressionSkyAtmosphereLightDirection, -900, 1800, light_index=0)
illuminance = node(material, E.MaterialExpressionSkyAtmosphereLightIlluminance, -900, 1900, light_index=0)

transmit = custom(material, -500, 1500, "DBBacklight", TRANSMIT, ["Subsurface", "CameraVector", "SunDirection", "Power", "Boost"])
link(subsurface_source, "", transmit, "Subsurface")
link(camera, "", transmit, "CameraVector")
link(sun, "", transmit, "SunDirection")
link(power, "", transmit, "Power")
link(boost, "", transmit, "Boost")
mel.connect_material_property(transmit, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)

rim = custom(material, -500, 1750, "DBBacklightGlow", GLOW, ["Subsurface", "CameraVector", "SunDirection", "Power", "Illuminance", "Glow"])
link(subsurface_source, "", rim, "Subsurface")
link(camera, "", rim, "CameraVector")
link(sun, "", rim, "SunDirection")
link(power, "", rim, "Power")
link(illuminance, "", rim, "Illuminance")
link(glow, "", rim, "Glow")
if emissive_source:
    total = node(material, E.MaterialExpressionAdd, -250, 1750)
    link(emissive_source, "", total, "A")
    link(rim, "", total, "B")
    mel.connect_material_property(total, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
else:
    mel.connect_material_property(rim, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

mel.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material, False)
unreal.log("DBBACKLIGHT patched %s" % MASTER)
