# Run inside the Unreal Editor Python environment (UE 5.8):
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_import_hyper3d.py
#
# Imports the authored character bodies made with Hyper3D Rodin and rigged by Tools/UE58/blender_rig_hyper3d.py:
#   SourceArt/Hyper3D/<Name>/export/SK_<Name>.fbx + T_<Name>_BaseColor/Normal/ORM.png
#   -> /Game/DarkBlood/Characters/Hyper3D/<Name>/SK_<Name> (on SK_Mannequin, the mannequin physics asset)
#      plus textures and MI_<Name> from M_DB_Character_PBR (created here when missing).
# The game picks them up by name: CV_Hyper3D_<Name> (DBDevelopmentContent) is the first body choice of the boss
# <Name> (DBBossDefinition) - without the import the bosses keep their Paragon or placeholder bodies.
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "SourceArt", "Hyper3D")
TARGET = "/Game/DarkBlood/Characters/Hyper3D"
MASTER = "/Game/DarkBlood/Art/Materials/Master/M_DB_Character_PBR"
SKELETON = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"
PHYSICS = "/Game/Characters/Mannequins/Rigs/PA_Mannequin"

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
E = unreal


def log(message):
    unreal.log("DBHYPER " + message)


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


DEFAULTS = {"BaseColor": "/Engine/EngineResources/WhiteSquareTexture", "Normal": "/Engine/EngineMaterials/FlatNormal",
            "ORM": "/Engine/EngineMaterials/T_Default_Material_Grid_M"}


def master_material():
    # Base color, tangent-space normal and glTF metallic-roughness (G roughness, B metallic) - what Rodin bakes.
    material = unreal.load_asset(MASTER)
    if material:
        # Masters made before the defaults existed: give their parameters a default texture and recompile.
        changed = False
        for expression in mel.get_material_expressions(material):
            texture = expression.get_editor_property("texture") if isinstance(expression, E.MaterialExpressionTextureSampleParameter2D) else None
            # Unset or Unreal's sRGB DefaultTexture: wrong for the normal and ORM samplers (the material then fails).
            if isinstance(expression, E.MaterialExpressionTextureSampleParameter2D) and (texture is None or texture.get_name() == "DefaultTexture"):
                name = str(expression.get_editor_property("parameter_name"))
                if name in DEFAULTS:
                    expression.set_editor_property("texture", unreal.load_asset(DEFAULTS[name]))
                    changed = True
        for expression in mel.get_material_expressions(material):
            if (isinstance(expression, E.MaterialExpressionTextureSampleParameter2D) and str(expression.get_editor_property("parameter_name")) == "ORM"
                    and expression.get_editor_property("sampler_type") != unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR):
                # ORM: linear color sampler, like its engine default texture; the ORM textures are imported linear.
                expression.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
                changed = True
        if changed:
            mel.recompile_material(material)
            library.save_loaded_asset(material, False)
            log("default textures added to " + MASTER)
        return material
    folder, name = MASTER.rsplit("/", 1)
    material = tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("used_with_skeletal_mesh", True)
    # Every texture parameter needs a default texture of its sampler type, or the material does not compile and the
    # body draws with the grey default material.
    base = node(material, E.MaterialExpressionTextureSampleParameter2D, -600, -200, parameter_name="BaseColor",
                texture=unreal.load_asset(DEFAULTS["BaseColor"]))
    normal = node(material, E.MaterialExpressionTextureSampleParameter2D, -600, 100, parameter_name="Normal",
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, texture=unreal.load_asset(DEFAULTS["Normal"]))
    orm = node(material, E.MaterialExpressionTextureSampleParameter2D, -600, 400, parameter_name="ORM",
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, texture=unreal.load_asset(DEFAULTS["ORM"]))
    tint = node(material, E.MaterialExpressionVectorParameter, -600, -420, parameter_name="Tint",
                default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    tinted = node(material, E.MaterialExpressionMultiply, -300, -260)
    mel.connect_material_expressions(base, "RGB", tinted, "A")
    mel.connect_material_expressions(tint, "", tinted, "B")
    mel.connect_material_property(tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(orm, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(orm, "B", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(material)
    library.save_loaded_asset(material, False)
    log("created " + MASTER)
    return material


def import_texture(path, destination, name, usage):
    imported = run_import(path, destination, name)
    texture = unreal.load_asset(imported[0]) if imported else None
    if texture is None:
        return None
    if usage == "Normal":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("srgb", False)
    elif usage == "ORM":
        # Linear color (not masks): matches the master's Linear Color sampler and its engine default texture.
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        texture.set_editor_property("srgb", False)
    library.save_loaded_asset(texture, False)
    return texture


def mesh_options(skeleton, physics):
    """The classic FBX importer: Interchange (UE 5.8) reads the bind pose of Blender's skinned FBX inconsistently."""
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
    data.set_editor_property("update_skeleton_reference_pose", False)
    data.set_editor_property("convert_scene", True)
    return options


skeleton = unreal.load_asset(SKELETON)
physics = unreal.load_asset(PHYSICS)
if skeleton is None:
    raise SystemExit("UE5 mannequin missing - run Tools/UE58/Setup-DevMannequin.ps1 first")
master = master_material()
for name in sorted(os.listdir(SOURCE)) if os.path.isdir(SOURCE) else []:
    export = os.path.join(SOURCE, name, "export")
    fbx = os.path.join(export, "SK_%s.fbx" % name)
    if not os.path.isfile(fbx):
        continue
    destination = TARGET + "/" + name
    textures = {}
    for usage in ("BaseColor", "Normal", "ORM"):
        path = os.path.join(export, "T_%s_%s.png" % (name, usage))
        if os.path.isfile(path):
            textures[usage] = import_texture(path, destination, "T_%s_%s" % (name, usage), usage)
    instance_path = destination + "/MI_" + name
    instance = unreal.load_asset(instance_path) or tools.create_asset(
        "MI_" + name, destination, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, master)
    for usage, texture in textures.items():
        if texture is not None:
            mel.set_material_instance_texture_parameter_value(instance, usage, texture)
    library.save_loaded_asset(instance, False)

    imported = run_import(fbx, destination, "SK_" + name, mesh_options(skeleton, physics))
    mesh = None
    for path in list(imported) + list(library.list_assets(destination, recursive=False, include_folder=False)):
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.SkeletalMesh):
            mesh = asset
            break
    if mesh is None:
        unreal.log_warning("DBHYPER %s: no skeletal mesh imported from %s" % (name, fbx))
        continue
    # Unreal's Python hands out copies of the slot structs: change the copies, write the whole list back.
    materials = []
    for slot in list(mesh.get_editor_property("materials")):
        slot.material_interface = instance
        materials.append(slot)
    unreal.SystemLibrary.execute_console_command(None, "Editor.AsyncSkinnedAssetCompilationFinishAll")
    mesh.set_editor_property("materials", materials)
    if physics is not None:
        mesh.set_editor_property("physics_asset", physics)
    library.save_loaded_asset(mesh, False)
    bounds = mesh.get_bounds()
    log("%s: %s on %s, %d material slots, height %.0f cm, textures %s" % (
        name, mesh.get_path_name(), mesh.get_editor_property("skeleton").get_name(), len(materials),
        bounds.box_extent.z * 2.0, ",".join(sorted(textures))))
log("done")
