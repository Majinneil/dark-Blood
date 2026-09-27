# Run inside the Unreal Editor Python environment (UE 5.8) after polyhaven_fetch.py:
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_import_polyhaven.py
#
# 1. Imports the Poly Haven textures (CC0) to /Game/DarkBlood/Art/Textures/PolyHaven/<id>/T_<id>_D|N|ARM
#    (normal maps flipped from OpenGL to DirectX, ARM linear = the masters' T_ORM).
#    Run db_create_material_foundation.py FIRST: it rebuilds the instances procedurally.
# 2. Imports the glTF models to /Game/DarkBlood/Art/Environment/PolyHaven/<id>/ (Nanite enabled).
# 3. Switches the material instances of the art kit to the texture path (UseTextures + WorldAligned).
# Re-running re-imports and re-assigns; instances not listed keep their procedural look (lacquer, paper, water ...).
import os
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "SourceArt", "PolyHaven")
TEXTURE_ROOT = "/Game/DarkBlood/Art/Textures/PolyHaven"
MODEL_ROOT = "/Game/DarkBlood/Art/Environment/PolyHaven"
INSTANCES = "/Game/DarkBlood/Art/Materials/Instances"
DARKBLOOD = "/Game/DarkBlood/Art/Materials/DarkBlood"

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(message):
    unreal.log("DBPH " + message)


def import_file(filename, destination, name):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    return [unreal.load_asset(path) for path in task.get_editor_property("imported_object_paths")]


# ---- 1. Textures ----------------------------------------------------------------------------------------
texture_dir = os.path.join(SOURCE, "Textures")
textures = {}
for asset in sorted(os.listdir(texture_dir)) if os.path.isdir(texture_dir) else []:
    folder = os.path.join(texture_dir, asset)
    destination = TEXTURE_ROOT + "/" + asset
    maps = {}
    for filename in sorted(os.listdir(folder)):
        path = os.path.join(folder, filename)
        if "_diff_" in filename:
            kind, suffix = "D", "D"
        elif "_nor_gl_" in filename:
            kind, suffix = "N", "N"
        elif "_arm_" in filename:
            kind, suffix = "ARM", "ARM"
        else:
            continue
        imported = [a for a in import_file(path, destination, "T_%s_%s" % (asset, suffix)) if isinstance(a, unreal.Texture2D)]
        if not imported:
            unreal.log_error("DBPH could not import " + path)
            continue
        texture = imported[0]
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
        if kind == "N":
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            texture.set_editor_property("srgb", False)
            texture.set_editor_property("flip_green_channel", True)
            texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
        elif kind == "ARM":
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
            texture.set_editor_property("srgb", False)
        library.save_loaded_asset(texture, False)
        maps[kind] = texture
    textures[asset] = maps
    log("textures %s (%d maps)" % (asset, len(maps)))

# ---- 2. Models --------------------------------------------------------------------------------------------
model_dir = os.path.join(SOURCE, "Models")
for asset in sorted(os.listdir(model_dir)) if os.path.isdir(model_dir) else []:
    folder = os.path.join(model_dir, asset)
    gltf = [f for f in os.listdir(folder) if f.endswith(".gltf")]
    if not gltf:
        continue
    destination = MODEL_ROOT + "/" + asset
    if library.does_directory_exist(destination):
        library.delete_directory(destination)
    imported = import_file(os.path.join(folder, gltf[0]), destination, "")
    meshes = []
    for path in library.list_assets(destination, recursive=True, include_folder=False):
        loaded = unreal.load_asset(path)
        if isinstance(loaded, unreal.MaterialInstanceConstant):
            # glTF "BLEND" leaves / moss become translucent, which Nanite does not render: use alpha-tested cut-outs.
            parent = loaded.get_editor_property("parent")
            if parent and "MI_Default_Blend" in parent.get_name():
                masked = unreal.load_asset(parent.get_path_name().replace("MI_Default_Blend", "MI_Default_Mask").split(".")[0])
                if masked:
                    mel.set_material_instance_parent(loaded, masked)
                    mel.update_material_instance(loaded)
                    log("masked material " + loaded.get_name())
        if isinstance(loaded, unreal.StaticMesh):
            settings = loaded.get_editor_property("nanite_settings")
            settings.set_editor_property("enabled", True)
            if asset.startswith(("tree", "shrub", "fern", "moss")):
                # Foliage cards: keep their area when Nanite simplifies, otherwise leaves vanish at distance.
                settings.set_editor_property("shape_preservation", unreal.NaniteShapePreservation.PRESERVE_AREA)
            loaded.set_editor_property("nanite_settings", settings)
            library.save_loaded_asset(loaded, False)
            meshes.append(path)
        elif loaded is not None:
            library.save_loaded_asset(loaded, False)
    log("model %s -> %s" % (asset, ", ".join(meshes)))

# ---- 2b. Foliage materials: two-sided foliage master + separate alpha map --------------------------------------
# model, imported material to replace, texture prefix in SourceArt/.../textures
FOLIAGE = [
    ("tree_small_02", "tree_small_02_leaves", "tree_small_02_leaves"),
    ("moss_01", "moss_01", "moss_01"),
    ("shrub_02", "shrub_02", "shrub_02"),
    ("shrub_04", "shrub_04", "shrub_04"),
    ("fern_02", "fern_02", "fern_02"),
]
foliage_master = unreal.load_asset("/Game/DarkBlood/Art/Materials/Master/M_DB_Foliage_Master")
for asset, material_name, prefix in FOLIAGE:
    folder = os.path.join(model_dir, asset, "textures")
    if not os.path.isdir(folder) or foliage_master is None:
        continue
    destination = MODEL_ROOT + "/" + asset + "/DB"
    maps = {}
    for filename in sorted(os.listdir(folder)):
        if not filename.startswith(prefix + "_"):
            continue
        rest = filename[len(prefix) + 1:]
        kind = "D" if rest.startswith("diff") else "N" if rest.startswith("nor_gl") else "ARM" if rest.startswith("arm") else "A" if rest.startswith("alpha") else None
        if kind is None:
            continue
        imported = [a for a in import_file(os.path.join(folder, filename), destination, "T_%s_%s" % (prefix, kind)) if isinstance(a, unreal.Texture2D)]
        if not imported:
            continue
        texture = imported[0]
        if kind == "N":
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            texture.set_editor_property("srgb", False)
            texture.set_editor_property("flip_green_channel", True)
        elif kind in ("ARM", "A"):
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
            texture.set_editor_property("srgb", False)
        library.save_loaded_asset(texture, False)
        maps[kind] = texture
    if len(maps) < 4:
        unreal.log_warning("DBPH foliage maps for %s incomplete (%s)" % (asset, ",".join(maps)))
        continue
    name = "MI_DB_Foliage_" + prefix
    path = destination + "/" + name
    if library.does_asset_exist(path):
        library.delete_asset(path)
    instance = tools.create_asset(name, destination, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, foliage_master)
    mel.set_material_instance_static_switch_parameter_value(instance, "UseTextures", True)
    mel.set_material_instance_static_switch_parameter_value(instance, "WorldAligned", False)
    mel.set_material_instance_texture_parameter_value(instance, "T_BaseColor", maps["D"])
    mel.set_material_instance_texture_parameter_value(instance, "T_Normal", maps["N"])
    mel.set_material_instance_texture_parameter_value(instance, "T_ORM", maps["ARM"])
    mel.set_material_instance_texture_parameter_value(instance, "T_Opacity", maps["A"])
    mel.set_material_instance_vector_parameter_value(instance, "TextureTint", unreal.LinearColor(0.9, 0.92, 0.85, 1.0))
    mel.set_material_instance_scalar_parameter_value(instance, "MacroStrength", 0.35)
    mel.set_material_instance_scalar_parameter_value(instance, "WindIntensity", 0.12 if asset == "tree_small_02" else 0.2)
    mel.update_material_instance(instance)
    library.save_loaded_asset(instance, False)
    replaced = 0
    for mesh_path in library.list_assets(MODEL_ROOT + "/" + asset, recursive=True, include_folder=False):
        mesh = unreal.load_asset(mesh_path)
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        materials = mesh.get_editor_property("static_materials")
        changed = False
        for index, slot in enumerate(materials):
            current = slot.get_editor_property("material_interface")
            if current is not None and current.get_name() == material_name:
                mesh.set_material(index, instance)
                changed = True
                replaced += 1
        if changed:
            library.save_loaded_asset(mesh, False)
    log("foliage %s -> %s (%d slots)" % (asset, name, replaced))

# ---- 3. Material instances -> texture path ---------------------------------------------------------------
# instance, folder, poly haven id, world size (cm), texture tint, extra scalars
ASSIGN = [
    ("MI_DB_Wood_Weathered_Dark", INSTANCES, "weathered_planks", 180.0, (0.8, 0.72, 0.66), {}),
    ("MI_DB_Wood_New_Light", INSTANCES, "hinoki_planks", 180.0, (1.0, 1.0, 1.0), {}),
    ("MI_DB_Wood_Wet", INSTANCES, "old_planks_02", 180.0, (0.7, 0.66, 0.6), {"Wetness": 0.45}),
    ("MI_DB_Wood_Burnt", INSTANCES, "weathered_planks", 180.0, (0.12, 0.1, 0.09), {}),
    ("MI_DB_Plaster_Lime", INSTANCES, "plastered_wall_02", 260.0, (1.0, 0.97, 0.92), {}),
    ("MI_DB_Plaster_Clay", INSTANCES, "clay_plaster", 260.0, (0.85, 0.78, 0.7), {}),
    ("MI_DB_Roof_Tile_Dark", INSTANCES, "grey_roof_01", 220.0, (0.55, 0.56, 0.6), {}),
    ("MI_DB_Roof_Thatch", INSTANCES, "reed_roof_04", 260.0, (0.9, 0.85, 0.75), {}),
    ("MI_DB_Stone_Temple", INSTANCES, "japanese_stone_wall", 320.0, (1.0, 1.0, 1.0), {}),
    ("MI_DB_Stone_Dry", INSTANCES, "japanese_stone_wall", 320.0, (0.85, 0.85, 0.85), {}),
    ("MI_DB_Stone_Wet", INSTANCES, "mossy_rock", 260.0, (0.75, 0.75, 0.75), {"Wetness": 0.5}),
    ("MI_DB_Stone_Mossy", INSTANCES, "mossy_rock", 260.0, (1.0, 1.0, 1.0), {}),
    ("MI_DB_Stone_Mountain", INSTANCES, "lichen_rock", 420.0, (0.9, 0.9, 0.9), {}),
    ("MI_DB_Stone_Ruin", INSTANCES, "rock_pitted_mossy", 320.0, (0.7, 0.68, 0.64), {}),
    ("MI_DB_Stone_Corrupted", INSTANCES, "lichen_rock", 320.0, (0.35, 0.3, 0.3), {}),
    ("MI_DB_Ground_Courtyard", INSTANCES, "grey_stone_path", 420.0, (0.9, 0.88, 0.85), {}),
    ("MI_DB_Ground_ForestFloor", INSTANCES, "forest_leaves_04", 320.0, (0.8, 0.8, 0.75), {}),
    ("MI_DB_Ground_PackedEarth", INSTANCES, "rocky_trail_02", 360.0, (0.85, 0.8, 0.75), {}),
    ("MI_DB_Bark_Cedar", INSTANCES, "japanese_cedar_bark", 140.0, (1.0, 1.0, 1.0), {}),
    ("MI_DB_Bark_Sakura", INSTANCES, "sakura_bark", 140.0, (1.0, 1.0, 1.0), {}),
    ("MI_DB_DarkBlood_Soil", DARKBLOOD, "burned_ground_01", 380.0, (0.6, 0.5, 0.48), {}),
    ("MI_DB_DarkBlood_Stone", DARKBLOOD, "lichen_rock", 320.0, (0.3, 0.25, 0.25), {}),
]
wood_master = unreal.load_asset("/Game/DarkBlood/Art/Materials/Master/M_DB_Wood_Master")
for name, folder, asset, world_size, tint, scalars in ASSIGN:
    path = folder + "/" + name
    instance = unreal.load_asset(path)
    if instance is None:
        # Bark instances are new; they derive from the wood master.
        instance = tools.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(instance, wood_master)
    maps = textures.get(asset)
    if not maps or len(maps) < 3:
        unreal.log_warning("DBPH textures for %s missing, %s keeps its procedural look" % (asset, name))
        continue
    mel.set_material_instance_static_switch_parameter_value(instance, "UseTextures", True)
    mel.set_material_instance_static_switch_parameter_value(instance, "WorldAligned", True)
    mel.set_material_instance_texture_parameter_value(instance, "T_BaseColor", maps["D"])
    mel.set_material_instance_texture_parameter_value(instance, "T_Normal", maps["N"])
    mel.set_material_instance_texture_parameter_value(instance, "T_ORM", maps["ARM"])
    mel.set_material_instance_scalar_parameter_value(instance, "TextureWorldSize", world_size)
    mel.set_material_instance_vector_parameter_value(instance, "TextureTint", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
    for key, value in scalars.items():
        mel.set_material_instance_scalar_parameter_value(instance, key, value)
    mel.update_material_instance(instance)
    library.save_loaded_asset(instance, False)
    log("instance %s <- %s" % (name, asset))

log("Poly Haven import complete")
