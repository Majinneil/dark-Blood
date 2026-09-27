# Run inside the Unreal Editor Python environment (UE 5.8):
#   UnrealEditor-Cmd.exe DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_create_material_foundation.py
#
# Creates the DARK BLOOD material foundation (docs/VISUAL_FOUNDATION.md):
#   /Game/DarkBlood/Art/Materials/Master     M_DB_*_Master (one shared surface graph, per-family defaults)
#   /Game/DarkBlood/Art/Materials/Landscape  M_DB_Landscape_Master (8 paint layers)
#   /Game/DarkBlood/Art/Materials/Instances  MI_DB_* variants used by the modular kit and the visual slice
#
# Every master works WITHOUT textures (tint + engine macro variation) so the greybox kit already reads as
# wood / stone / plaster. Assign CC0 textures (Poly Haven, see 02_ASSET_MANIFEST_AI_SAFE.csv) on an instance and
# enable "UseTextures" to switch to the texture path. Re-running the script rebuilds the masters and instances.
import unreal

MASTER = "/Game/DarkBlood/Art/Materials/Master"
LANDSCAPE = "/Game/DarkBlood/Art/Materials/Landscape"
INSTANCES = "/Game/DarkBlood/Art/Materials/Instances"
DARKBLOOD = "/Game/DarkBlood/Art/Materials/DarkBlood"

mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
E = unreal

MACRO_TEX = unreal.load_asset("/Engine/EngineMaterials/T_Default_MacroVariation")
DIFFUSE_TEX = unreal.load_asset("/Engine/EngineMaterials/DefaultDiffuse")
NORMAL_TEX = unreal.load_asset("/Engine/EngineMaterials/FlatNormal")
MASKS_TEX = unreal.load_asset("/Engine/EngineMaterials/T_Default_Material_Grid_M")
WORLD_ALIGNED_TEX = unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture")
WORLD_ALIGNED_NORMAL = unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal")
GRASS_WIND = unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/WorldPositionOffset/SimpleGrassWind")

warnings = []


def color(r, g, b, a=1.0):
    return unreal.LinearColor(r, g, b, a)


class Graph:
    """Small helper around MaterialEditingLibrary that lays nodes out in columns and logs failed links."""

    def __init__(self, material):
        self.material = material
        self.count = 0

    def node(self, cls, **props):
        x = -2400 + (self.count // 18) * 260
        y = -900 + (self.count % 18) * 110
        self.count += 1
        expression = mel.create_material_expression(self.material, cls, x, y)
        for key, value in props.items():
            expression.set_editor_property(key, value)
        return expression

    def link(self, source, target, pin="", out=""):
        if not mel.connect_material_expressions(source, out, target, pin):
            warnings.append("%s: %s.%s -> %s.%s" % (self.material.get_name(), source.get_name(), out, target.get_name(), pin))

    def output(self, source, prop, out=""):
        if not mel.connect_material_property(source, out, prop):
            warnings.append("%s: output %s" % (self.material.get_name(), prop))

    # parameters
    def scalar(self, name, value, group):
        return self.node(E.MaterialExpressionScalarParameter, parameter_name=name, default_value=value, group=group)

    def vector(self, name, value, group):
        return self.node(E.MaterialExpressionVectorParameter, parameter_name=name, default_value=value, group=group)

    def switch(self, name, value, group, when_true, when_false, out_true="", out_false=""):
        expression = self.node(E.MaterialExpressionStaticSwitchParameter, parameter_name=name, default_value=value, group=group)
        self.link(when_true, expression, "True", out_true)
        self.link(when_false, expression, "False", out_false)
        return expression

    def texture_param(self, name, texture, sampler, group):
        return self.node(E.MaterialExpressionTextureObjectParameter, parameter_name=name, texture=texture, sampler_type=sampler, group=group)

    # math
    def binary(self, cls, a, b, pin_a="A", pin_b="B", const_a="const_a", const_b="const_b"):
        expression = self.node(cls)
        if isinstance(a, (int, float)):
            expression.set_editor_property(const_a, float(a))
        else:
            self.link(a, expression, pin_a)
        if isinstance(b, (int, float)):
            expression.set_editor_property(const_b, float(b))
        else:
            self.link(b, expression, pin_b)
        return expression

    def mul(self, a, b):
        return self.binary(E.MaterialExpressionMultiply, a, b)

    def add(self, a, b):
        return self.binary(E.MaterialExpressionAdd, a, b)

    def sub(self, a, b):
        return self.binary(E.MaterialExpressionSubtract, a, b)

    def div(self, a, b):
        return self.binary(E.MaterialExpressionDivide, a, b)

    def lerp(self, a, b, alpha):
        expression = self.node(E.MaterialExpressionLinearInterpolate)
        for value, pin, const in ((a, "A", "const_a"), (b, "B", "const_b"), (alpha, "Alpha", "const_alpha")):
            if isinstance(value, (int, float)):
                expression.set_editor_property(const, float(value))
            else:
                self.link(value, expression, pin)
        return expression

    def unary(self, cls, value, **props):
        expression = self.node(cls, **props)
        self.link(value, expression)
        return expression

    def saturate(self, value):
        return self.unary(E.MaterialExpressionSaturate, value)

    def mask(self, value, r=False, g=False, b=False, a=False):
        return self.unary(E.MaterialExpressionComponentMask, value, r=r, g=g, b=b, a=a)

    def append(self, a, b):
        return self.binary(E.MaterialExpressionAppendVector, a, b)

    def sample(self, texture, uvs, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR):
        expression = self.node(E.MaterialExpressionTextureSample, texture=texture, sampler_type=sampler)
        self.link(uvs, expression, "UVs")
        return expression

    def function(self, asset):
        return self.node(E.MaterialExpressionMaterialFunctionCall, material_function=asset)


def new_asset(name, folder, cls, factory):
    path = folder + "/" + name
    if library.does_asset_exist(path):
        library.delete_asset(path)
    return tools.create_asset(name, folder, cls, factory)


def set_usage(material):
    for flag in ("used_with_skeletal_mesh", "used_with_instanced_static_meshes", "used_with_nanite", "used_with_static_lighting"):
        try:
            material.set_editor_property(flag, True)
        except Exception:
            pass


def projected_uv(g, position, scale):
    """World position -> 2D coordinates that also vary on vertical faces, divided by a world-space scale."""
    x = g.mask(position, r=True)
    y = g.mask(position, g=True)
    z = g.mask(position, b=True)
    u = g.add(x, g.mul(z, 0.7))
    v = g.sub(y, g.mul(z, 0.7))
    return g.div(g.append(u, v), scale)


def build_surface_master(name, d):
    """Shared DARK BLOOD surface graph. d = per-family defaults / feature flags."""
    material = new_asset(name, MASTER, unreal.Material, unreal.MaterialFactoryNew())
    set_usage(material)
    if d.get("foliage"):
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        material.set_editor_property("two_sided", True)
    if d.get("subsurface"):
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SUBSURFACE)
    g = Graph(material)

    world = g.node(E.MaterialExpressionWorldPosition)

    # --- procedural base (no textures): tint + two-scale macro variation + stretched detail variation
    base_tint = g.vector("BaseTint", d["tint"], "01 Color")
    tint_variation = g.vector("TintVariation", d["tint2"], "01 Color")
    macro_scale = g.scalar("MacroScale", d.get("macro_scale", 1500.0), "02 Variation")
    macro_strength = g.scalar("MacroStrength", d.get("macro_strength", 0.6), "02 Variation")
    detail_scale = g.scalar("DetailScale", d.get("detail_scale", 140.0), "02 Variation")
    detail_strength = g.scalar("DetailStrength", d.get("detail_strength", 0.3), "02 Variation")
    grain = g.vector("GrainStretch", d.get("grain", color(1, 1, 1)), "02 Variation")

    macro_uv = projected_uv(g, world, macro_scale)
    macro_a = g.sample(MACRO_TEX, macro_uv)
    macro_b = g.sample(MACRO_TEX, g.mul(macro_uv, 0.213))
    macro = g.saturate(g.mul(g.mul(g.mask(macro_a, r=True), g.mask(macro_b, g=True)), 2.2))
    macro_alpha = g.mul(macro, macro_strength)

    stretched = g.mul(world, g.mask(grain, r=True, g=True, b=True))
    detail = g.mask(g.sample(MACRO_TEX, projected_uv(g, stretched, detail_scale)), b=True)
    detail_factor = g.lerp(g.sub(1.0, detail_strength), 1.0, detail)

    procedural_color = g.mul(g.lerp(base_tint, tint_variation, macro_alpha), detail_factor)
    rough_min = g.scalar("RoughnessMin", d.get("rough", (0.6, 0.85))[0], "03 Surface")
    rough_max = g.scalar("RoughnessMax", d.get("rough", (0.6, 0.85))[1], "03 Surface")
    procedural_rough = g.lerp(rough_min, rough_max, detail)

    # --- texture path (CC0 PBR sets): UV0 tiling or world-aligned projection for the modular kit
    t_color = g.texture_param("T_BaseColor", DIFFUSE_TEX, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "04 Textures")
    t_normal = g.texture_param("T_Normal", NORMAL_TEX, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, "04 Textures")
    t_orm = g.texture_param("T_ORM", MASKS_TEX, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, "04 Textures")
    texture_tint = g.vector("TextureTint", color(1, 1, 1), "04 Textures")
    world_size = g.scalar("TextureWorldSize", 200.0, "04 Textures")
    uv_tiling = g.scalar("UVTiling", 1.0, "04 Textures")

    uv = g.mul(g.node(E.MaterialExpressionTextureCoordinate), uv_tiling)
    uv_color = g.node(E.MaterialExpressionTextureSample)
    g.link(t_color, uv_color, "Tex")
    g.link(uv, uv_color, "UVs")
    uv_normal = g.node(E.MaterialExpressionTextureSample, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    g.link(t_normal, uv_normal, "Tex")
    g.link(uv, uv_normal, "UVs")
    uv_orm = g.node(E.MaterialExpressionTextureSample, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    g.link(t_orm, uv_orm, "Tex")
    g.link(uv, uv_orm, "UVs")

    size3 = g.append(g.append(world_size, world_size), world_size)
    wa_color = g.function(WORLD_ALIGNED_TEX)
    g.link(t_color, wa_color, "TextureObject")
    g.link(size3, wa_color, "TextureSize")
    wa_orm = g.function(WORLD_ALIGNED_TEX)
    g.link(t_orm, wa_orm, "TextureObject")
    g.link(size3, wa_orm, "TextureSize")
    wa_normal = g.function(WORLD_ALIGNED_NORMAL)
    g.link(t_normal, wa_normal, "TextureObject")
    g.link(size3, wa_normal, "TextureSize")
    wa_normal_tangent = g.node(E.MaterialExpressionTransform, transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
                               transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_TANGENT)
    g.link(wa_normal, wa_normal_tangent, "", "XYZ Texture")

    tex_color = g.switch("WorldAligned", False, "04 Textures", wa_color, uv_color, "XYZ Texture", "RGB")
    tex_orm = g.switch("WorldAligned", False, "04 Textures", wa_orm, uv_orm, "XYZ Texture", "RGB")
    tex_normal = g.switch("WorldAligned", False, "04 Textures", wa_normal_tangent, uv_normal, "", "RGB")
    textured_color = g.mul(g.mul(tex_color, texture_tint), g.lerp(1.0, 0.72, macro_alpha))
    textured_rough = g.mask(tex_orm, g=True)

    flat_normal = g.node(E.MaterialExpressionConstant3Vector, constant=color(0, 0, 1))
    surface_color = g.switch("UseTextures", False, "04 Textures", textured_color, procedural_color)
    roughness = g.switch("UseTextures", False, "04 Textures", textured_rough, procedural_rough)
    normal = g.switch("UseTextures", False, "04 Textures", tex_normal, flat_normal)
    ao = g.switch("UseTextures", False, "04 Textures", g.mask(tex_orm, r=True), g.node(E.MaterialExpressionConstant, r=1.0))

    # --- dirt (cavity-like, from inverted detail) and moss on upward faces (world-aligned blend)
    dirt_amount = g.scalar("DirtAmount", d.get("dirt", 0.0), "05 Weathering")
    dirt_color = g.vector("DirtColor", color(0.09, 0.075, 0.06), "05 Weathering")
    dirt_mask = g.saturate(g.mul(dirt_amount, g.sub(1.0, detail)))
    weathered = g.lerp(surface_color, dirt_color, dirt_mask)

    moss_amount = g.scalar("MossAmount", d.get("moss", 0.0), "05 Weathering")
    moss_color = g.vector("MossColor", color(0.07, 0.11, 0.035), "05 Weathering")
    moss_height = g.scalar("MossHeight", 0.35, "05 Weathering")
    normal_z = g.mask(g.node(E.MaterialExpressionVertexNormalWS), b=True)
    moss_mask = g.saturate(g.mul(g.mul(g.saturate(g.mul(g.sub(normal_z, moss_height), 4.0)), moss_amount), g.lerp(0.35, 1.3, macro)))
    mossy_color = g.switch("EnableMoss", d.get("moss_switch", False), "05 Weathering", g.lerp(weathered, moss_color, moss_mask), weathered)
    mossy_rough = g.switch("EnableMoss", d.get("moss_switch", False), "05 Weathering", g.lerp(roughness, 0.95, moss_mask), roughness)

    # --- wetness: darker albedo, glossy
    wetness = g.scalar("Wetness", d.get("wet", 0.0), "05 Weathering")
    wet_color = g.mul(mossy_color, g.lerp(1.0, 0.5, wetness))
    wet_rough = g.lerp(mossy_rough, 0.08, wetness)

    # --- DARK BLOOD corruption layer: darkened tint + pulsing emissive veins (local accents, not a red filter)
    corruption = g.scalar("CorruptionAmount", d.get("corruption", 0.0), "06 Dark Blood")
    corrupt_tint = g.vector("CorruptTint", color(0.05, 0.012, 0.012), "06 Dark Blood")
    vein_color = g.vector("VeinColor", color(1.0, 0.05, 0.03), "06 Dark Blood")
    vein_glow = g.scalar("VeinGlow", 12.0, "06 Dark Blood")
    vein_scale = g.scalar("VeinScale", 180.0, "06 Dark Blood")
    vein_sharpness = g.scalar("VeinSharpness", 9.0, "06 Dark Blood")
    pulse_speed = g.scalar("PulseSpeed", 1.4, "06 Dark Blood")
    noise = g.node(E.MaterialExpressionNoise, scale=1.0, levels=3, output_min=0.0, output_max=1.0, quality=1)
    g.link(g.div(world, vein_scale), noise, "World Position")
    ridge = g.saturate(g.sub(1.0, g.mul(g.unary(E.MaterialExpressionAbs, g.sub(g.mul(noise, 2.0), 1.0)), vein_sharpness)))
    veins = g.mul(g.binary(E.MaterialExpressionPower, ridge, 2.0, "Base", "Exponent", const_b="const_exponent"), corruption)
    pulse = g.lerp(0.35, 1.0, g.add(g.mul(g.unary(E.MaterialExpressionSine, g.mul(g.node(E.MaterialExpressionTime), pulse_speed), period=6.283), 0.5), 0.5))
    corrupt_color = g.lerp(g.lerp(wet_color, corrupt_tint, g.mul(corruption, 0.8)), 0.0, veins)
    corrupt_emissive = g.mul(g.mul(g.mul(vein_color, vein_glow), veins), pulse)
    final_color = g.switch("EnableCorruption", d.get("corruption_switch", False), "06 Dark Blood", corrupt_color, wet_color)
    zero = g.node(E.MaterialExpressionConstant, r=0.0)
    vein_emissive = g.switch("EnableCorruption", d.get("corruption_switch", False), "06 Dark Blood", corrupt_emissive, zero)

    # --- emissive (lanterns, shoji at night, embers)
    emissive_color = g.vector("EmissiveColor", d.get("emissive", color(0, 0, 0)), "07 Emissive")
    emissive_strength = g.scalar("EmissiveStrength", d.get("emissive_strength", 0.0), "07 Emissive")
    emissive = g.add(g.mul(emissive_color, emissive_strength), vein_emissive)

    metallic = g.scalar("Metallic", d.get("metallic", 0.0), "03 Surface")
    specular = g.scalar("Specular", d.get("specular", 0.5), "03 Surface")

    g.output(final_color, unreal.MaterialProperty.MP_BASE_COLOR)
    g.output(wet_rough, unreal.MaterialProperty.MP_ROUGHNESS)
    g.output(metallic, unreal.MaterialProperty.MP_METALLIC)
    g.output(specular, unreal.MaterialProperty.MP_SPECULAR)
    g.output(normal, unreal.MaterialProperty.MP_NORMAL)
    g.output(ao, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    g.output(emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    if d.get("foliage"):
        opacity = g.switch("UseTextures", False, "04 Textures", uv_color, g.node(E.MaterialExpressionConstant, r=1.0), "A", "")
        g.output(opacity, unreal.MaterialProperty.MP_OPACITY_MASK)
        subsurface = g.vector("SubsurfaceColor", color(0.08, 0.12, 0.02), "03 Surface")
        g.output(subsurface, unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
        wind = g.function(GRASS_WIND)
        g.link(g.scalar("WindIntensity", 0.25, "08 Wind"), wind, "WindIntensity")
        g.link(g.scalar("WindWeight", 0.6, "08 Wind"), wind, "WindWeight")
        g.link(g.scalar("WindSpeed", 0.4, "08 Wind"), wind, "WindSpeed")
        g.link(g.node(E.MaterialExpressionConstant3Vector, constant=color(0, 0, 0)), wind, "AdditionalWPO")
        g.output(wind, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    if d.get("subsurface"):
        g.output(g.vector("SubsurfaceColor", color(0.7, 0.25, 0.18), "03 Surface"), unreal.MaterialProperty.MP_SUBSURFACE_COLOR)

    mel.layout_material_expressions(material)
    mel.recompile_material(material)
    library.save_loaded_asset(material, False)
    unreal.log("DBMAT master " + name)
    return material


def build_decal_master():
    material = new_asset("M_DB_Decal_Master", MASTER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    g = Graph(material)
    t_color = g.texture_param("T_BaseColor", DIFFUSE_TEX, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "04 Textures")
    sample = g.node(E.MaterialExpressionTextureSample)
    g.link(t_color, sample, "Tex")
    tint = g.vector("BaseTint", color(0.08, 0.05, 0.04), "01 Color")
    opacity = g.scalar("Opacity", 0.8, "01 Color")
    g.output(g.mul(g.mask(sample, r=True, g=True, b=True), tint), unreal.MaterialProperty.MP_BASE_COLOR)
    g.output(g.mul(g.mask(sample, a=True), opacity), unreal.MaterialProperty.MP_OPACITY)
    g.output(g.scalar("Roughness", 0.8, "03 Surface"), unreal.MaterialProperty.MP_ROUGHNESS)
    mel.layout_material_expressions(material)
    mel.recompile_material(material)
    library.save_loaded_asset(material, False)
    unreal.log("DBMAT master M_DB_Decal_Master")


LANDSCAPE_LAYERS = [
    # name, tint, variation, roughness
    ("ForestFloor", color(0.075, 0.062, 0.035), color(0.045, 0.06, 0.025), 0.9),
    ("Earth", color(0.16, 0.12, 0.085), color(0.11, 0.085, 0.06), 0.85),
    ("StonePath", color(0.26, 0.25, 0.23), color(0.18, 0.175, 0.165), 0.7),
    ("Rock", color(0.2, 0.2, 0.2), color(0.13, 0.13, 0.135), 0.75),
    ("Mud", color(0.07, 0.05, 0.035), color(0.05, 0.04, 0.03), 0.35),
    ("Moss", color(0.06, 0.1, 0.03), color(0.04, 0.07, 0.02), 0.9),
    ("Ash", color(0.09, 0.085, 0.085), color(0.04, 0.035, 0.035), 0.95),
    ("Snow", color(0.8, 0.82, 0.86), color(0.65, 0.68, 0.74), 0.5),
]


def build_landscape_master():
    material = new_asset("M_DB_Landscape_Master", LANDSCAPE, unreal.Material, unreal.MaterialFactoryNew())
    g = Graph(material)
    world = g.node(E.MaterialExpressionWorldPosition)
    macro_scale = g.scalar("MacroScale", 3000.0, "02 Variation")
    macro_uv = projected_uv(g, world, macro_scale)
    macro = g.saturate(g.mul(g.mul(g.mask(g.sample(MACRO_TEX, macro_uv), r=True), g.mask(g.sample(MACRO_TEX, g.mul(macro_uv, 0.19)), g=True)), 2.2))
    detail = g.mask(g.sample(MACRO_TEX, projected_uv(g, world, g.scalar("DetailScale", 180.0, "02 Variation"))), b=True)
    detail_factor = g.lerp(0.7, 1.0, detail)

    color_blend = g.node(E.MaterialExpressionLandscapeLayerBlend)
    rough_blend = g.node(E.MaterialExpressionLandscapeLayerBlend)
    def make_layers():
        layers = []
        for index, (name, tint, variation, rough) in enumerate(LANDSCAPE_LAYERS):
            layer = unreal.LayerBlendInput()
            layer.set_editor_property("layer_name", name)
            layer.set_editor_property("blend_type", unreal.LandscapeLayerBlendType.LB_WEIGHT_BLEND)
            layer.set_editor_property("preview_weight", 1.0 if index == 0 else 0.0)
            layers.append(layer)
        return layers

    color_blend.set_editor_property("layers", make_layers())
    rough_blend.set_editor_property("layers", make_layers())
    for name, tint, variation, rough in LANDSCAPE_LAYERS:
        layer_color = g.mul(g.lerp(g.vector(name + "_Tint", tint, "Layer " + name), g.vector(name + "_Variation", variation, "Layer " + name), macro), detail_factor)
        g.link(layer_color, color_blend, "Layer " + name)
        g.link(g.scalar(name + "_Roughness", rough, "Layer " + name), rough_blend, "Layer " + name)
    g.output(color_blend, unreal.MaterialProperty.MP_BASE_COLOR)
    g.output(rough_blend, unreal.MaterialProperty.MP_ROUGHNESS)
    mel.layout_material_expressions(material)
    mel.recompile_material(material)
    library.save_loaded_asset(material, False)
    unreal.log("DBMAT master M_DB_Landscape_Master")


MASTERS = {
    "M_DB_Wood_Master": dict(tint=color(0.14, 0.085, 0.05), tint2=color(0.09, 0.055, 0.035), rough=(0.55, 0.8), grain=color(0.12, 0.12, 1.0),
                             detail_scale=60.0, detail_strength=0.4, dirt=0.2, moss_switch=True),
    "M_DB_Stone_Master": dict(tint=color(0.3, 0.29, 0.27), tint2=color(0.2, 0.2, 0.195), rough=(0.65, 0.9), detail_strength=0.35, dirt=0.25,
                              moss_switch=True, corruption_switch=True),
    "M_DB_Plaster_Master": dict(tint=color(0.7, 0.66, 0.58), tint2=color(0.55, 0.5, 0.43), rough=(0.8, 0.95), detail_strength=0.18, dirt=0.3,
                                macro_strength=0.45),
    "M_DB_Roof_Master": dict(tint=color(0.1, 0.1, 0.11), tint2=color(0.07, 0.07, 0.08), rough=(0.45, 0.7), grain=color(1.0, 1.0, 1.0), detail_scale=40.0,
                             detail_strength=0.45, moss_switch=True),
    "M_DB_Metal_Master": dict(tint=color(0.35, 0.34, 0.33), tint2=color(0.22, 0.2, 0.19), rough=(0.3, 0.6), metallic=1.0, detail_strength=0.25),
    "M_DB_Fabric_Master": dict(tint=color(0.55, 0.5, 0.42), tint2=color(0.45, 0.4, 0.33), rough=(0.85, 1.0), detail_scale=20.0, detail_strength=0.2,
                               specular=0.3),
    "M_DB_Foliage_Master": dict(tint=color(0.028, 0.048, 0.016), tint2=color(0.04, 0.052, 0.018), rough=(0.7, 0.9), foliage=True),
    "M_DB_Wetness_Master": dict(tint=color(0.02, 0.035, 0.035), tint2=color(0.015, 0.03, 0.028), rough=(0.05, 0.1), wet=0.0, specular=0.8,
                                macro_strength=0.3),
    "M_DB_Skin_Support": dict(tint=color(0.6, 0.42, 0.33), tint2=color(0.52, 0.35, 0.28), rough=(0.45, 0.6), detail_strength=0.08, subsurface=True),
}


def build_darkblood_master():
    d = dict(tint=color(0.06, 0.035, 0.03), tint2=color(0.03, 0.015, 0.015), rough=(0.3, 0.7), corruption=1.0, corruption_switch=True, moss_switch=False)
    material = build_surface_master("M_DB_DarkBlood_Master", d)
    return material


INSTANCES_TABLE = [
    # name, parent, folder, vectors, scalars, switches
    ("MI_DB_Wood_Weathered_Dark", "M_DB_Wood_Master", INSTANCES, {"BaseTint": color(0.085, 0.055, 0.035), "TintVariation": color(0.05, 0.035, 0.025)}, {"DirtAmount": 0.35}, {}),
    ("MI_DB_Wood_New_Light", "M_DB_Wood_Master", INSTANCES, {"BaseTint": color(0.42, 0.28, 0.16), "TintVariation": color(0.33, 0.21, 0.12)}, {"DirtAmount": 0.05}, {}),
    ("MI_DB_Wood_Wet", "M_DB_Wood_Master", INSTANCES, {"BaseTint": color(0.1, 0.065, 0.04)}, {"Wetness": 0.6, "MossAmount": 0.4}, {}),
    ("MI_DB_Wood_Burnt", "M_DB_Wood_Master", INSTANCES, {"BaseTint": color(0.02, 0.018, 0.016), "TintVariation": color(0.05, 0.03, 0.02)}, {"RoughnessMin": 0.8, "RoughnessMax": 1.0}, {}),
    ("MI_DB_Wood_Lacquer_Red", "M_DB_Wood_Master", INSTANCES, {"BaseTint": color(0.36, 0.04, 0.025), "TintVariation": color(0.26, 0.03, 0.02)}, {"RoughnessMin": 0.3, "RoughnessMax": 0.5, "DetailStrength": 0.12, "DirtAmount": 0.15}, {}),
    ("MI_DB_Wood_Lacquer_Black", "M_DB_Wood_Master", INSTANCES, {"BaseTint": color(0.02, 0.018, 0.017), "TintVariation": color(0.03, 0.025, 0.022)}, {"RoughnessMin": 0.25, "RoughnessMax": 0.45, "DetailStrength": 0.1}, {}),
    ("MI_DB_Stone_Dry", "M_DB_Stone_Master", INSTANCES, {}, {}, {}),
    ("MI_DB_Stone_Wet", "M_DB_Stone_Master", INSTANCES, {}, {"Wetness": 0.55}, {}),
    ("MI_DB_Stone_Mossy", "M_DB_Stone_Master", INSTANCES, {}, {"MossAmount": 1.0, "DirtAmount": 0.4}, {}),
    ("MI_DB_Stone_Temple", "M_DB_Stone_Master", INSTANCES, {"BaseTint": color(0.42, 0.4, 0.36), "TintVariation": color(0.33, 0.31, 0.28)}, {"DetailStrength": 0.22}, {}),
    ("MI_DB_Stone_Mountain", "M_DB_Stone_Master", INSTANCES, {"BaseTint": color(0.22, 0.22, 0.23), "TintVariation": color(0.14, 0.14, 0.15)}, {"MacroScale": 4000.0}, {}),
    ("MI_DB_Stone_Ruin", "M_DB_Stone_Master", INSTANCES, {"BaseTint": color(0.24, 0.23, 0.21)}, {"DirtAmount": 0.6, "MossAmount": 0.6}, {}),
    ("MI_DB_Stone_Corrupted", "M_DB_Stone_Master", INSTANCES, {"BaseTint": color(0.12, 0.1, 0.1)}, {"CorruptionAmount": 0.6, "VeinSharpness": 14.0, "VeinGlow": 6.0}, {}),
    ("MI_DB_Plaster_Lime", "M_DB_Plaster_Master", INSTANCES, {}, {}, {}),
    ("MI_DB_Plaster_Clay", "M_DB_Plaster_Master", INSTANCES, {"BaseTint": color(0.36, 0.27, 0.18), "TintVariation": color(0.28, 0.2, 0.13)}, {"DirtAmount": 0.35}, {}),
    ("MI_DB_Paper_Shoji", "M_DB_Fabric_Master", INSTANCES, {"BaseTint": color(0.78, 0.74, 0.64), "TintVariation": color(0.7, 0.66, 0.56), "EmissiveColor": color(1.0, 0.62, 0.3)}, {"EmissiveStrength": 0.0}, {}),
    ("MI_DB_Paper_Shoji_Lit", "M_DB_Fabric_Master", INSTANCES, {"BaseTint": color(0.78, 0.74, 0.64), "EmissiveColor": color(1.0, 0.6, 0.28)}, {"EmissiveStrength": 1.4}, {}),
    ("MI_DB_Roof_Tile_Dark", "M_DB_Roof_Master", INSTANCES, {}, {"MossAmount": 0.25}, {}),
    ("MI_DB_Roof_Thatch", "M_DB_Roof_Master", INSTANCES, {"BaseTint": color(0.23, 0.18, 0.1), "TintVariation": color(0.15, 0.12, 0.07)}, {"RoughnessMin": 0.85, "RoughnessMax": 1.0, "MossAmount": 0.5}, {}),
    ("MI_DB_Roof_Copper", "M_DB_Roof_Master", INSTANCES, {"BaseTint": color(0.12, 0.26, 0.2), "TintVariation": color(0.08, 0.18, 0.14)}, {"RoughnessMin": 0.4, "RoughnessMax": 0.6}, {}),
    ("MI_DB_Metal_Iron_Dark", "M_DB_Metal_Master", INSTANCES, {"BaseTint": color(0.12, 0.115, 0.11)}, {"RoughnessMin": 0.45, "RoughnessMax": 0.75}, {}),
    ("MI_DB_Metal_Bronze", "M_DB_Metal_Master", INSTANCES, {"BaseTint": color(0.45, 0.3, 0.14), "TintVariation": color(0.25, 0.22, 0.12)}, {}, {}),
    ("MI_DB_Fabric_Linen", "M_DB_Fabric_Master", INSTANCES, {}, {}, {}),
    ("MI_DB_Fabric_Crimson", "M_DB_Fabric_Master", INSTANCES, {"BaseTint": color(0.3, 0.025, 0.025), "TintVariation": color(0.2, 0.02, 0.02)}, {}, {}),
    ("MI_DB_Fabric_Indigo", "M_DB_Fabric_Master", INSTANCES, {"BaseTint": color(0.03, 0.04, 0.12), "TintVariation": color(0.02, 0.03, 0.08)}, {}, {}),
    ("MI_DB_Lantern_Paper", "M_DB_Fabric_Master", INSTANCES, {"BaseTint": color(0.8, 0.55, 0.3), "EmissiveColor": color(1.0, 0.45, 0.15)}, {"EmissiveStrength": 6.0}, {}),
    ("MI_DB_Lantern_Fire", "M_DB_Fabric_Master", INSTANCES, {"BaseTint": color(0.9, 0.6, 0.3), "EmissiveColor": color(1.0, 0.5, 0.18)}, {"EmissiveStrength": 20.0}, {}),
    ("MI_DB_Water_Stream", "M_DB_Wetness_Master", INSTANCES, {}, {}, {}),
    ("MI_DB_Ground_PackedEarth", "M_DB_Stone_Master", INSTANCES, {"BaseTint": color(0.15, 0.115, 0.08), "TintVariation": color(0.1, 0.08, 0.055)}, {"MacroScale": 2500.0, "DetailScale": 220.0, "RoughnessMin": 0.8, "RoughnessMax": 0.95}, {"EnableMoss": False}),
    ("MI_DB_Ground_ForestFloor", "M_DB_Stone_Master", INSTANCES, {"BaseTint": color(0.075, 0.06, 0.035), "TintVariation": color(0.05, 0.065, 0.028)}, {"MacroScale": 2000.0, "DetailScale": 90.0, "DetailStrength": 0.45, "RoughnessMin": 0.85, "RoughnessMax": 1.0}, {"EnableMoss": False}),
    ("MI_DB_Ground_Courtyard", "M_DB_Stone_Master", INSTANCES, {"BaseTint": color(0.33, 0.315, 0.29), "TintVariation": color(0.25, 0.24, 0.225)}, {"DetailStrength": 0.25, "DirtAmount": 0.3}, {"EnableMoss": False}),
    ("MI_DB_Foliage_Leaves", "M_DB_Foliage_Master", INSTANCES, {}, {}, {}),
    ("MI_DB_Foliage_Sakura", "M_DB_Foliage_Master", INSTANCES, {"BaseTint": color(0.62, 0.3, 0.38), "TintVariation": color(0.5, 0.2, 0.3), "SubsurfaceColor": color(0.5, 0.15, 0.2)}, {}, {}),
    ("MI_DB_Foliage_Dead", "M_DB_Foliage_Master", INSTANCES, {"BaseTint": color(0.08, 0.06, 0.04), "TintVariation": color(0.05, 0.04, 0.03)}, {}, {}),
    ("MI_DB_DarkBlood_Veins", "M_DB_DarkBlood_Master", DARKBLOOD, {}, {}, {}),
    ("MI_DB_DarkBlood_Soil", "M_DB_DarkBlood_Master", DARKBLOOD, {"BaseTint": color(0.05, 0.03, 0.025)}, {"CorruptionAmount": 0.35, "VeinScale": 520.0, "VeinSharpness": 16.0, "VeinGlow": 5.0}, {}),
    ("MI_DB_DarkBlood_Stone", "M_DB_DarkBlood_Master", DARKBLOOD, {"BaseTint": color(0.1, 0.085, 0.085)}, {"CorruptionAmount": 0.5, "Wetness": 0.3, "VeinSharpness": 14.0, "VeinGlow": 6.0}, {}),
    ("MI_DB_Skin_Default", "M_DB_Skin_Support", INSTANCES, {}, {}, {}),
    ("MI_DB_Decal_Grime", "M_DB_Decal_Master", INSTANCES, {}, {}, {}),
    ("MI_DB_Decal_BloodStain", "M_DB_Decal_Master", DARKBLOOD, {"BaseTint": color(0.12, 0.005, 0.005)}, {}, {}),
]


def build_instances():
    for name, parent, folder, vectors, scalars, switches in INSTANCES_TABLE:
        parent_path = (LANDSCAPE if parent == "M_DB_Landscape_Master" else MASTER) + "/" + parent
        parent_asset = unreal.load_asset(parent_path)
        if not parent_asset:
            warnings.append("missing parent " + parent_path)
            continue
        instance = new_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(instance, parent_asset)
        for key, value in vectors.items():
            mel.set_material_instance_vector_parameter_value(instance, key, value)
        for key, value in scalars.items():
            mel.set_material_instance_scalar_parameter_value(instance, key, value)
        for key, value in switches.items():
            mel.set_material_instance_static_switch_parameter_value(instance, key, value)
        mel.update_material_instance(instance)
        library.save_loaded_asset(instance, False)
        unreal.log("DBMAT instance " + name)


for folder in (MASTER, LANDSCAPE, INSTANCES, DARKBLOOD):
    if not library.does_directory_exist(folder):
        library.make_directory(folder)

for master_name, defaults in MASTERS.items():
    build_surface_master(master_name, defaults)
build_darkblood_master()
build_decal_master()
build_landscape_master()
build_instances()

for warning in warnings:
    unreal.log_warning("DBMAT " + warning)
unreal.log("DBMAT material foundation complete (%d warnings)" % len(warnings))
