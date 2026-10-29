# Run inside the Unreal Editor Python environment (UE 5.8):
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_import_akaza_outfit.py
#
# Imports Akaza's outfit (docs/AKAZA_OUTFIT.md) made by Tools/Art/blender_make_akaza_outfit.py and
# Tools/Art/db_make_kimono_fabrics.py from SourceArt/Generated/AkazaOutfit:
#   /Game/DarkBlood/Art/Materials/Master/M_DB_Cloth_Master   fabric/leather/brass: texture set, tiled weave detail, low specular for cloth
#   /Game/DarkBlood/Characters/Akaza/Outfit/Textures/T_*     the fabric textures
#   /Game/DarkBlood/Characters/Akaza/Outfit/MI_Akaza_*       one instance per material slot of the FBX
#   /Game/DarkBlood/Characters/Akaza/Outfit/SK_Akaza_Outfit  on SK_Mannequin - it follows any body on the UE5 skeleton
# The game adds SK_Akaza_Outfit to Akaza's profiles by name (DBDevelopmentContent); without the import nothing changes.
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "SourceArt", "Generated", "AkazaOutfit")
TARGET = "/Game/DarkBlood/Characters/Akaza/Outfit"
MASTER = "/Game/DarkBlood/Art/Materials/Master/M_DB_Cloth_Master"
SKELETON = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"
PHYSICS = "/Game/Characters/Mannequins/Rigs/PA_Mannequin"

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
E = unreal

# Slot name -> texture set, tint, roughness floor, metallic, cloth amount, weave detail strength.
SLOTS = {
    "MI_Akaza_KimonoIvory": ("Fabric_IvoryPattern", (1.0, 1.0, 1.0), 0.72, 0.0, 0.7, 0.5),
    "MI_Akaza_RobeBlack": ("Fabric_BlackMotif", (1.0, 1.0, 1.0), 0.6, 0.0, 0.8, 0.45),
    "MI_Akaza_HemBorder": ("Fabric_BlackBorder", (1.0, 1.0, 1.0), 0.6, 0.0, 0.8, 0.45),
    "MI_Akaza_BlackCotton": ("Fabric_BlackPlain", (1.0, 1.0, 1.0), 0.75, 0.0, 0.9, 0.6),
    "MI_Akaza_Leather": ("Leather_Brown", (1.0, 1.0, 1.0), 0.45, 0.0, 0.0, 0.0),
    "MI_Akaza_BootLeather": ("Leather_Brown", (0.32, 0.3, 0.3), 0.45, 0.0, 0.0, 0.0),
    "MI_Akaza_Rope": ("Rope_Hemp", (1.0, 1.0, 1.0), 0.8, 0.0, 0.5, 0.0),
    "MI_Akaza_Tassel": ("Rope_Hemp", (1.9, 1.8, 1.6), 0.85, 0.0, 0.8, 0.0),
    "MI_Akaza_Brass": (None, (0.5, 0.38, 0.2), 0.42, 1.0, 0.0, 0.0),
}


def log(message):
    unreal.log("DBOUTFIT " + message)


def run_import(filename, destination, name, options=None):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    if options is not None:
        task.set_editor_property("options", options)
    tools.import_asset_tasks([task])
    return list(task.get_editor_property("imported_object_paths"))


def node(material, cls, x, y, **props):
    expression = mel.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def link(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        unreal.log_warning("DBOUTFIT link failed: %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def scalar(material, name, value, x, y):
    return node(material, E.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def master_material(rough_default):
    """Base color with tint, normal with a finely tiled weave detail, roughness with a floor, low specular for cloth
    (Cloth 1 = 0.25, Cloth 0 = 0.5 for leather and brass), metallic for buckle and rings."""
    folder, name = MASTER.rsplit("/", 1)
    material = unreal.load_asset(MASTER)
    if material:
        # Rebuild the graph in place: the instances keep their parent.
        for expression in list(mel.get_material_expressions(material)):
            mel.delete_material_expression(material, expression)
    else:
        material = tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("used_with_skeletal_mesh", True)
    # Default lit: the Cloth shading model through material attributes compiled to nothing in the game (UE 5.8,
    # Substrate) and the outfit drew with the grey default material. Cloth reads through low specular instead.
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property("use_material_attributes", False)
    coords = node(material, E.MaterialExpressionTextureCoordinate, -1400, 0)
    base = node(material, E.MaterialExpressionTextureSampleParameter2D, -1000, -400, parameter_name="BaseColor",
                texture=unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    normal = node(material, E.MaterialExpressionTextureSampleParameter2D, -1000, -50, parameter_name="Normal",
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
                  texture=unreal.load_asset("/Engine/EngineMaterials/FlatNormal"))
    # Roughness maps are grayscale, linear: the sampler must say so, or the material fails to compile (grey default).
    rough = node(material, E.MaterialExpressionTextureSampleParameter2D, -1000, 300, parameter_name="Roughness",
                 sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE, texture=rough_default)
    for sampler in (base, normal, rough):
        link(coords, "", sampler, "UVs")
    # Weave detail: the same UVs, repeated DetailTiling times, added to the normal's slope.
    tiling = scalar(material, "DetailTiling", 40.0, -1400, 200)
    detail_uv = node(material, E.MaterialExpressionMultiply, -1200, 200)
    link(coords, "", detail_uv, "A")
    link(tiling, "", detail_uv, "B")
    detail = node(material, E.MaterialExpressionTextureSampleParameter2D, -1000, 650, parameter_name="DetailNormal",
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
                  texture=unreal.load_asset("/Engine/EngineMaterials/FlatNormal"))
    link(detail_uv, "", detail, "UVs")
    strength = scalar(material, "DetailStrength", 0.5, -1000, 950)
    detail_rg = node(material, E.MaterialExpressionComponentMask, -750, 650, r=True, g=True, b=False, a=False)
    link(detail, "RGB", detail_rg, "")
    detail_scaled = node(material, E.MaterialExpressionMultiply, -600, 650)
    link(detail_rg, "", detail_scaled, "A")
    link(strength, "", detail_scaled, "B")
    detail_3 = node(material, E.MaterialExpressionAppendVector, -450, 650)
    zero = node(material, E.MaterialExpressionConstant, -600, 800, r=0.0)
    link(detail_scaled, "", detail_3, "A")
    link(zero, "", detail_3, "B")
    combined = node(material, E.MaterialExpressionAdd, -300, 0)
    link(normal, "RGB", combined, "A")
    link(detail_3, "", combined, "B")
    normalized = node(material, E.MaterialExpressionNormalize, -150, 0)
    link(combined, "", normalized, "")
    mel.connect_material_property(normalized, "", unreal.MaterialProperty.MP_NORMAL)
    # Color.
    tint = node(material, E.MaterialExpressionVectorParameter, -1000, -650, parameter_name="Tint",
                default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    tinted = node(material, E.MaterialExpressionMultiply, -600, -450)
    link(base, "RGB", tinted, "A")
    link(tint, "", tinted, "B")
    mel.connect_material_property(tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)
    # Cloth scatters light: little specular (leather and brass keep the default 0.5 through Cloth = 0).
    cloth = scalar(material, "Cloth", 0.7, -600, -650)
    specular = node(material, E.MaterialExpressionLinearInterpolate, -300, -500)
    half = node(material, E.MaterialExpressionConstant, -450, -560, r=0.5)
    low = node(material, E.MaterialExpressionConstant, -450, -500, r=0.25)
    link(half, "", specular, "A")
    link(low, "", specular, "B")
    link(cloth, "", specular, "Alpha")
    mel.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)
    # Roughness with a floor; metallic.
    floor = scalar(material, "RoughnessMin", 0.6, -750, 450)
    rough_max = node(material, E.MaterialExpressionMax, -500, 300)
    link(rough, "R", rough_max, "A")
    link(floor, "", rough_max, "B")
    mel.connect_material_property(rough_max, "", unreal.MaterialProperty.MP_ROUGHNESS)
    metal = scalar(material, "Metallic", 0.0, -300, 450)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(material)
    library.save_loaded_asset(material, False)
    log("master " + MASTER)
    return material


def import_texture(stem, usage):
    path = os.path.join(SOURCE, "T_%s.png" % stem)
    if not os.path.isfile(path):
        return None
    imported = run_import(path, TARGET + "/Textures", "T_" + stem)
    texture = unreal.load_asset(imported[0]) if imported else None
    if texture is None:
        return None
    if usage == "Normal":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("srgb", False)
    elif usage == "Gray":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
        texture.set_editor_property("srgb", False)
    library.save_loaded_asset(texture, False)
    return texture


def mesh_options(skeleton, physics):
    """The classic FBX importer: Interchange (UE 5.8) reads the bind pose of Blender's skinned FBX inconsistently
    ("BindPose ... FbxCluster vs. FbxPose") and the imported outfit then drew nowhere in the game."""
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX false")
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", True)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    options.set_editor_property("skeleton", skeleton)
    options.set_editor_property("physics_asset", physics)
    options.set_editor_property("create_physics_asset", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", False)
    data = options.get_editor_property("skeletal_mesh_import_data")
    data.set_editor_property("import_morph_targets", False)
    data.set_editor_property("use_t0_as_ref_pose", False)
    data.set_editor_property("update_skeleton_reference_pose", False)
    data.set_editor_property("convert_scene", True)
    data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    # The FBX slots are named like the instances (MI_Akaza_*): the importer finds them next to the mesh.
    options.get_editor_property("texture_import_data").set_editor_property(
        "material_search_location", unreal.MaterialSearchLocation.LOCAL)
    return options


fbx = os.path.join(SOURCE, "SK_Akaza_Outfit.fbx")
skeleton = unreal.load_asset(SKELETON)
if skeleton is None:
    raise SystemExit("UE5 mannequin missing - run Tools/UE58/Setup-DevMannequin.ps1 first")
if not os.path.isfile(fbx):
    raise SystemExit("no outfit - run Tools/Art/blender_make_akaza_outfit.py first")
weave = import_texture("Weave_N", "Normal")
textures = {"Fabric_BlackPlain_R": import_texture("Fabric_BlackPlain_R", "Gray")}
master = master_material(textures["Fabric_BlackPlain_R"])
instances = {}
for slot_name, (stem, tint, rough_min, metallic, cloth, detail) in SLOTS.items():
    instance_path = TARGET + "/" + slot_name
    instance = unreal.load_asset(instance_path) or tools.create_asset(
        slot_name, TARGET, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, master)
    if stem:
        for suffix, usage, parameter in (("BC", "Color", "BaseColor"), ("N", "Normal", "Normal"), ("R", "Gray", "Roughness")):
            key = "%s_%s" % (stem, suffix)
            if key not in textures:
                textures[key] = import_texture(key, usage)
            if textures[key] is not None:
                mel.set_material_instance_texture_parameter_value(instance, parameter, textures[key])
    if weave is not None and detail > 0.0:
        mel.set_material_instance_texture_parameter_value(instance, "DetailNormal", weave)
    mel.set_material_instance_vector_parameter_value(instance, "Tint", unreal.LinearColor(*tint, 1.0))
    mel.set_material_instance_scalar_parameter_value(instance, "RoughnessMin", rough_min)
    mel.set_material_instance_scalar_parameter_value(instance, "Metallic", metallic)
    mel.set_material_instance_scalar_parameter_value(instance, "Cloth", cloth)
    mel.set_material_instance_scalar_parameter_value(instance, "DetailStrength", detail)
    library.save_loaded_asset(instance, False)
    instances[slot_name] = instance
log("%d material instances, %d textures" % (len(instances), sum(1 for t in textures.values() if t)))

imported = run_import(fbx, TARGET, "SK_Akaza_Outfit", mesh_options(skeleton, unreal.load_asset(PHYSICS)))
mesh = None
for path in list(imported) + list(library.list_assets(TARGET, recursive=False, include_folder=False)):
    asset = unreal.load_asset(path)
    if isinstance(asset, unreal.SkeletalMesh):
        mesh = asset
        break
if mesh is None:
    raise SystemExit("DBOUTFIT no skeletal mesh imported from " + fbx)
# Unreal's Python hands out copies of the slot structs (iterating or indexing the array): change copies, write the
# whole list back.
slots = []
assigned = 0
for slot in list(mesh.get_editor_property("materials")):
    name = str(slot.get_editor_property("material_slot_name"))
    if name in instances:
        slot.material_interface = instances[name]
        assigned += 1
    else:
        unreal.log_warning("DBOUTFIT slot without instance: " + name)
    slots.append(slot)
# The mesh builds in the background after the import: let it finish first, then assign and save.
unreal.SystemLibrary.execute_console_command(None, "Editor.AsyncSkinnedAssetCompilationFinishAll")
mesh.set_editor_property("materials", slots)
library.save_loaded_asset(mesh, False)
check = [slot.get_editor_property("material_interface").get_name() for slot in mesh.get_editor_property("materials")]
if any(not name.startswith("MI_Akaza_") for name in check):
    unreal.log_warning("DBOUTFIT slots not assigned: %s" % check)
for path in library.list_assets(TARGET, recursive=True, include_folder=False):
    library.save_asset(path, only_if_is_dirty=True)
log("SK_Akaza_Outfit imported: %d material slots assigned, skeleton %s" % (assigned, skeleton.get_name()))
