# Run inside the Unreal Editor Python environment (UE 5.8) after polyhaven_fetch.py:
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_import_polyhaven_world.py
#
# World pass of the Poly Haven selection (CC0): backdrop terrain, cliffs, grass, harbor and tavern props.
# Imports only the assets listed here (same folders and naming as db_import_polyhaven.py) and creates the terrain
# material instances. Safe to re-run.
import os
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "SourceArt", "PolyHaven")
TEXTURE_ROOT = "/Game/DarkBlood/Art/Textures/PolyHaven"
MODEL_ROOT = "/Game/DarkBlood/Art/Environment/PolyHaven"
INSTANCES = "/Game/DarkBlood/Art/Materials/Instances"

TEXTURES = ["aerial_grass_rock", "aerial_rocks_02", "snow_02", "cliff_side", "leafy_grass", "coast_sand_rocks_02"]
MODELS = ["coastal_cliff_01", "coastal_cliff_02", "rock_face_01", "rock_face_02", "grass_medium_01", "grass_medium_02",
          "dutch_ship_medium", "ship_pinnace", "modular_wooden_pier", "wooden_crate_02", "wooden_barrels_01", "chinese_tea_table",
          "round_wooden_table_01", "wooden_stool_02", "lantern_chandelier_01"]
FOLIAGE = ("grass",)

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(message):
    unreal.log("DBPHW " + message)


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
textures = {}
for asset in TEXTURES:
    folder = os.path.join(SOURCE, "Textures", asset)
    if not os.path.isdir(folder):
        unreal.log_warning("DBPHW missing textures " + asset)
        continue
    maps = {}
    for filename in sorted(os.listdir(folder)):
        kind = "D" if "_diff_" in filename else "N" if "_nor_gl_" in filename else "ARM" if "_arm_" in filename else None
        if kind is None:
            continue
        imported = [a for a in import_file(os.path.join(folder, filename), TEXTURE_ROOT + "/" + asset, "T_%s_%s" % (asset, kind))
                    if isinstance(a, unreal.Texture2D)]
        if not imported:
            continue
        texture = imported[0]
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
        if kind == "N":
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            texture.set_editor_property("srgb", False)
            texture.set_editor_property("flip_green_channel", True)
            texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
        elif kind == "ARM":
            texture.set_editor_property("srgb", False)
        library.save_loaded_asset(texture, False)
        maps[kind] = texture
    textures[asset] = maps
    log("textures %s (%d maps)" % (asset, len(maps)))

# ---- 2. Models (Nanite) ------------------------------------------------------------------------------------
SKIP_MODELS = os.environ.get("DBPHW_SKIP_MODELS") == "1"  # re-run materials only
for asset in [] if SKIP_MODELS else MODELS:
    folder = os.path.join(SOURCE, "Models", asset)
    gltf = [f for f in os.listdir(folder) if f.endswith(".gltf")] if os.path.isdir(folder) else []
    if not gltf:
        unreal.log_warning("DBPHW missing model " + asset)
        continue
    destination = MODEL_ROOT + "/" + asset
    if library.does_directory_exist(destination):
        library.delete_directory(destination)
    import_file(os.path.join(folder, gltf[0]), destination, "")
    meshes = []
    for path in library.list_assets(destination, recursive=True, include_folder=False):
        loaded = unreal.load_asset(path)
        if isinstance(loaded, unreal.MaterialInstanceConstant):
            parent = loaded.get_editor_property("parent")
            if parent and "MI_Default_Blend" in parent.get_name():
                masked = unreal.load_asset(parent.get_path_name().replace("MI_Default_Blend", "MI_Default_Mask").split(".")[0])
                if masked:
                    mel.set_material_instance_parent(loaded, masked)
                    mel.update_material_instance(loaded)
        if isinstance(loaded, unreal.StaticMesh):
            settings = loaded.get_editor_property("nanite_settings")
            settings.set_editor_property("enabled", True)
            if asset.startswith(FOLIAGE):
                settings.set_editor_property("shape_preservation", unreal.NaniteShapePreservation.PRESERVE_AREA)
            loaded.set_editor_property("nanite_settings", settings)
            bounds = loaded.get_bounding_box()
            meshes.append("%s [%.0f x %.0f x %.0f cm]" % (loaded.get_name(), bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y,
                                                          bounds.max.z - bounds.min.z))
        if loaded is not None:
            library.save_loaded_asset(loaded, False)
    log("model %s -> %s" % (asset, ", ".join(meshes)))

# ---- 2b. Grass: two-sided foliage master with the separate alpha map (as db_import_polyhaven.py does for leaves) --
foliage_master = unreal.load_asset("/Game/DarkBlood/Art/Materials/Master/M_DB_Foliage_Master")
for asset in ("grass_medium_01", "grass_medium_02"):
    folder = os.path.join(SOURCE, "Models", asset, "textures")
    if not os.path.isdir(folder) or foliage_master is None:
        continue
    destination = MODEL_ROOT + "/" + asset + "/DB"
    maps = {}
    for filename in sorted(os.listdir(folder)):
        rest = filename[len(asset) + 1:]
        kind = "D" if rest.startswith("diff") else "N" if rest.startswith("nor_gl") else "ARM" if rest.startswith("arm") else "A" if rest.startswith("alpha") else None
        if kind is None:
            continue
        imported = [a for a in import_file(os.path.join(folder, filename), destination, "T_%s_%s" % (asset, kind)) if isinstance(a, unreal.Texture2D)]
        if not imported:
            continue
        texture = imported[0]
        if kind == "N":
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            texture.set_editor_property("srgb", False)
            texture.set_editor_property("flip_green_channel", True)
        elif kind in ("ARM", "A"):
            texture.set_editor_property("srgb", False)
        library.save_loaded_asset(texture, False)
        maps[kind] = texture
    if len(maps) < 4:
        unreal.log_warning("DBPHW grass maps for %s incomplete" % asset)
        continue
    name = "MI_DB_Foliage_" + asset
    instance = unreal.load_asset(destination + "/" + name) or tools.create_asset(name, destination, unreal.MaterialInstanceConstant,
                                                                                    unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, foliage_master)
    mel.set_material_instance_static_switch_parameter_value(instance, "UseTextures", True)
    mel.set_material_instance_static_switch_parameter_value(instance, "WorldAligned", False)
    mel.set_material_instance_texture_parameter_value(instance, "T_BaseColor", maps["D"])
    mel.set_material_instance_texture_parameter_value(instance, "T_Normal", maps["N"])
    mel.set_material_instance_texture_parameter_value(instance, "T_ORM", maps["ARM"])
    mel.set_material_instance_texture_parameter_value(instance, "T_Opacity", maps["A"])
    mel.set_material_instance_vector_parameter_value(instance, "TextureTint", unreal.LinearColor(0.75, 1.1, 0.55, 1.0))
    mel.set_material_instance_scalar_parameter_value(instance, "MacroStrength", 0.45)
    mel.set_material_instance_scalar_parameter_value(instance, "WindIntensity", 0.35)
    mel.update_material_instance(instance)
    library.save_loaded_asset(instance, False)
    replaced = 0
    for mesh_path in library.list_assets(MODEL_ROOT + "/" + asset, recursive=True, include_folder=False):
        mesh = unreal.load_asset(mesh_path)
        if isinstance(mesh, unreal.StaticMesh):
            for index in range(len(mesh.get_editor_property("static_materials"))):
                mesh.set_material(index, instance)
                replaced += 1
            library.save_loaded_asset(mesh, False)
    log("grass %s -> %s (%d slots)" % (asset, name, replaced))

# ---- 3. Terrain instances (stone master, world-aligned; "moss" on upward faces doubles as snow / grass) -------
stone_master = unreal.load_asset("/Game/DarkBlood/Art/Materials/Master/M_DB_Stone_Master")
# name, texture set, world size (cm), tint, cover (color, amount, height) or None, extra scalars
TERRAIN = [
    ("MI_DB_Terrain_Mountain", "aerial_rocks_02", 6000.0, (0.55, 0.55, 0.6), ((0.66, 0.69, 0.75), 1.0, 0.55), {"MacroScale": 60000.0}),
    ("MI_DB_Terrain_Hills", "aerial_grass_rock", 5000.0, (0.8, 0.85, 0.75), ((0.05, 0.09, 0.035), 0.6, 0.6), {"MacroScale": 30000.0}),
    ("MI_DB_Terrain_Cliff", "cliff_side", 900.0, (0.75, 0.73, 0.7), ((0.06, 0.1, 0.04), 0.7, 0.72), {"MacroScale": 8000.0}),
    ("MI_DB_Ground_Meadow", "leafy_grass", 300.0, (0.5, 0.78, 0.34), None, {"MacroStrength": 0.55, "MacroScale": 4000.0}),
    ("MI_DB_Ground_Shore", "coast_sand_rocks_02", 400.0, (0.9, 0.88, 0.84), None, {}),
    ("MI_DB_Terrain_Snow", "snow_02", 500.0, (0.74, 0.76, 0.82), None, {"MacroScale": 30000.0}),
]
for name, asset, world_size, tint, cover, scalars in TERRAIN:
    maps = textures.get(asset)
    if not maps or len(maps) < 3 or stone_master is None:
        unreal.log_warning("DBPHW %s skipped (textures %s missing)" % (name, asset))
        continue
    path = INSTANCES + "/" + name
    instance = unreal.load_asset(path) or tools.create_asset(name, INSTANCES, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, stone_master)
    mel.set_material_instance_static_switch_parameter_value(instance, "UseTextures", True)
    mel.set_material_instance_static_switch_parameter_value(instance, "WorldAligned", True)
    mel.set_material_instance_static_switch_parameter_value(instance, "EnableMoss", cover is not None)
    mel.set_material_instance_texture_parameter_value(instance, "T_BaseColor", maps["D"])
    mel.set_material_instance_texture_parameter_value(instance, "T_Normal", maps["N"])
    mel.set_material_instance_texture_parameter_value(instance, "T_ORM", maps["ARM"])
    mel.set_material_instance_scalar_parameter_value(instance, "TextureWorldSize", world_size)
    mel.set_material_instance_vector_parameter_value(instance, "TextureTint", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
    if cover is not None:
        color, amount, height = cover
        mel.set_material_instance_vector_parameter_value(instance, "MossColor", unreal.LinearColor(color[0], color[1], color[2], 1.0))
        mel.set_material_instance_scalar_parameter_value(instance, "MossAmount", amount)
        mel.set_material_instance_scalar_parameter_value(instance, "MossHeight", height)
    for key, value in scalars.items():
        mel.set_material_instance_scalar_parameter_value(instance, key, value)
    mel.update_material_instance(instance)
    library.save_loaded_asset(instance, False)
    log("instance %s <- %s" % (name, asset))

# ---- 4. Blood river of the demon lands: dark wet surface with a red glow (no textures) --------------------------
wet_master = unreal.load_asset("/Game/DarkBlood/Art/Materials/Master/M_DB_Wetness_Master")
if wet_master is not None:
    folder = "/Game/DarkBlood/Art/Materials/DarkBlood"
    name = "MI_DB_Blood_River"
    instance = unreal.load_asset(folder + "/" + name) or tools.create_asset(name, folder, unreal.MaterialInstanceConstant,
                                                                            unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, wet_master)
    for key, value in {"BaseTint": (0.06, 0.004, 0.004), "TintVariation": (0.14, 0.01, 0.008), "EmissiveColor": (1.0, 0.045, 0.02)}.items():
        mel.set_material_instance_vector_parameter_value(instance, key, unreal.LinearColor(value[0], value[1], value[2], 1.0))
    for key, value in {"EmissiveStrength": 2.2, "Wetness": 1.0, "RoughnessMin": 0.02, "RoughnessMax": 0.08}.items():
        mel.set_material_instance_scalar_parameter_value(instance, key, value)
    mel.update_material_instance(instance)
    library.save_loaded_asset(instance, False)
    log("instance " + name)

# ---- 4b. Sea of the open world: deep blue-green, mirror-smooth (Lumen / ray traced reflections) --------------------
if wet_master is not None:
    name = "MI_DB_Water_Sea"
    instance = unreal.load_asset(INSTANCES + "/" + name) or tools.create_asset(name, INSTANCES, unreal.MaterialInstanceConstant,
                                                                               unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, wet_master)
    for key, value in {"BaseTint": (0.01, 0.045, 0.06), "TintVariation": (0.015, 0.06, 0.07)}.items():
        mel.set_material_instance_vector_parameter_value(instance, key, unreal.LinearColor(value[0], value[1], value[2], 1.0))
    for key, value in {"Wetness": 1.0, "RoughnessMin": 0.02, "RoughnessMax": 0.06, "MacroScale": 20000.0}.items():
        mel.set_material_instance_scalar_parameter_value(instance, key, value)
    mel.update_material_instance(instance)
    library.save_loaded_asset(instance, False)
    log("instance " + name)

# ---- 5. Demon tree leaves: the island tree's leaf cards, desaturated to a dark blood red with a faint glow -----
foliage_master = unreal.load_asset("/Game/DarkBlood/Art/Materials/Master/M_DB_Foliage_Master")
leaf_root = MODEL_ROOT + "/island_tree_02/DB/T_island_tree_02_leaves_"
leaf_maps = {kind: unreal.load_asset(leaf_root + kind) for kind in ("D", "N", "ARM", "A")}
if foliage_master is not None and all(leaf_maps.values()):
    folder = "/Game/DarkBlood/Art/Materials/DarkBlood"
    name = "MI_DB_Foliage_Demon_Leaves"
    instance = unreal.load_asset(folder + "/" + name) or tools.create_asset(name, folder, unreal.MaterialInstanceConstant,
                                                                            unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, foliage_master)
    mel.set_material_instance_static_switch_parameter_value(instance, "UseTextures", True)
    mel.set_material_instance_static_switch_parameter_value(instance, "WorldAligned", False)
    for key, kind in (("T_BaseColor", "D"), ("T_Normal", "N"), ("T_ORM", "ARM"), ("T_Opacity", "A")):
        mel.set_material_instance_texture_parameter_value(instance, key, leaf_maps[kind])
    mel.set_material_instance_scalar_parameter_value(instance, "TextureDesaturation", 1.0)
    for key, value in {"TextureTint": (0.55, 0.035, 0.03), "SubsurfaceColor": (0.6, 0.02, 0.02), "EmissiveColor": (1.0, 0.05, 0.02)}.items():
        mel.set_material_instance_vector_parameter_value(instance, key, unreal.LinearColor(value[0], value[1], value[2], 1.0))
    mel.set_material_instance_scalar_parameter_value(instance, "EmissiveStrength", 0.6)
    mel.set_material_instance_scalar_parameter_value(instance, "WindIntensity", 0.1)
    mel.update_material_instance(instance)
    library.save_loaded_asset(instance, False)
    log("instance " + name)
else:
    unreal.log_warning("DBPHW demon leaves skipped (island_tree_02 leaf textures missing)")

log("Poly Haven world import complete")
