# Run inside the Unreal Editor Python environment (UE 5.8):
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_import_fab.py
#
# Imports the free Fab assets (all "Allows usage with AI: Yes") that the user downloaded from THEIR Fab library
# into SourceArt/Fab (see docs/VISUAL_FOUNDATION_STATUS.md for the list and licenses):
#   CC BY 4.0 models  -> /Game/DarkBlood/Art/Fab/...        (committed, attribution in docs/CREDITS.md)
#   Standard-license animations -> /Game/DarkBlood/Dev/FabAnims/... (NOT committed: Fab forbids standalone
#   redistribution; every developer imports them from their own library)
# The UE5 mannequin (Tools/UE58/Setup-DevMannequin.ps1) must exist: rolls use its skeleton, the fight pack is
# retargeted onto it with an auto-generated IK Rig / IK Retargeter.
import os
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "SourceArt", "Fab")
ART = "/Game/DarkBlood/Art/Fab"
ANIMS = "/Game/DarkBlood/Dev/FabAnims"
MANNY_SKELETON = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"
MANNY_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary


def log(message):
    unreal.log("DBFAB " + message)


def run_import(filename, destination, name="", options=None):
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


def assets_of(folder, cls):
    found = []
    for path in library.list_assets(folder, recursive=True, include_folder=False):
        asset = unreal.load_asset(path)
        if isinstance(asset, cls):
            found.append(asset)
    return found


def save_folder(folder):
    for path in library.list_assets(folder, recursive=True, include_folder=False):
        asset = unreal.load_asset(path)
        if asset is not None:
            library.save_loaded_asset(asset, False)


# ---- 1. CC BY models -------------------------------------------------------------------------------------
MODELS = [
    ("japanese_torii_gate/scene.gltf", "Torii_Pikas"),
    ("medieval_wall_mounted_lantern/scene.gltf", "WallLantern_Kigha"),
    ("ibaraki_temple_statue/scene.gltf", "Statue_Ibaraki_MTSU"),
    ("shrine_statue_kusatsu/source/Scaniverse/Scaniverse.obj", "Statue_Kusatsu_MTSU"),
]
for relative, name in MODELS:
    source = os.path.join(SOURCE, relative)
    if not os.path.isfile(source):
        unreal.log_warning("DBFAB missing " + source)
        continue
    destination = ART + "/" + name
    # Re-imports overwrite in place (deleting first leaves materials pointing at stale texture packages).
    run_import(source, destination)
    for mesh in assets_of(destination, unreal.StaticMesh):
        settings = mesh.get_editor_property("nanite_settings")
        settings.set_editor_property("enabled", True)
        mesh.set_editor_property("nanite_settings", settings)
        extent = mesh.get_bounds().box_extent
        log("model %s mesh %s extent %.1f %.1f %.1f" % (name, mesh.get_path_name(), extent.x, extent.y, extent.z))
    save_folder(destination)


# ---- 2. Animations ---------------------------------------------------------------------------------------
def fbx_anim_options(skeleton, import_mesh=False):
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", import_mesh)
    options.set_editor_property("import_as_skeletal", True)
    options.set_editor_property("import_animations", True)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    options.set_editor_property("automated_import_should_detect_type", False)
    if skeleton is not None:
        options.set_editor_property("skeleton", skeleton)
    return options


def interchange_anim_options(skeleton):
    # Animation-only FBX onto an existing skeleton through the Interchange pipeline (the legacy options are ignored).
    pipeline = unreal.InterchangeGenericAssetsPipeline()
    common = pipeline.get_editor_property("common_skeletal_meshes_and_animations_properties")
    common.set_editor_property("import_only_animations", True)
    common.set_editor_property("skeleton", skeleton)
    anim = pipeline.get_editor_property("animation_pipeline")
    anim.set_editor_property("import_animations", True)
    override = unreal.InterchangePipelineStackOverride()
    override.add_pipeline(pipeline)
    return override


def make_montage(sequence, folder, name, auto_blend_out=True):
    sequence.set_editor_property("force_root_lock", True)
    library.save_loaded_asset(sequence, False)
    path = folder + "/" + name
    if library.does_asset_exist(path):
        library.delete_asset(path)
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property("source_animation", sequence)
    montage = tools.create_asset(name, folder, unreal.AnimMontage, factory)
    if montage:
        montage.set_editor_property("enable_auto_blend_out", auto_blend_out)
        library.save_loaded_asset(montage, False)
        log("montage " + path)
    return montage


manny_skeleton = unreal.load_asset(MANNY_SKELETON)
manny_mesh = unreal.load_asset(MANNY_MESH)
if manny_skeleton is None or manny_mesh is None:
    unreal.log_error("DBFAB UE5 mannequin missing - run Tools/UE58/Setup-DevMannequin.ps1 first")
else:
    # 2a. Rolls: authored on the UE5 mannequin skeleton -> direct import, one montage per direction.
    roll_dir = os.path.join(SOURCE, "falling_rolling")
    roll_folder = ANIMS + "/Rolls"
    ROLL_ORDER = ["front", "front_right_45", "right", "back_right_45", "back", "back_left_45", "left", "front_left_45"]
    if os.path.isdir(roll_dir):
        for index, direction in enumerate(ROLL_ORDER):
            source = os.path.join(roll_dir, "RM_Roll_%s.fbx" % direction)
            if not os.path.isfile(source):
                continue
            paths = run_import(source, roll_folder, "A_DB_Roll_%s" % direction, fbx_anim_options(manny_skeleton))
            sequences = [unreal.load_asset(p) for p in paths]
            sequences = [s for s in sequences if isinstance(s, unreal.AnimSequence)]
            if sequences:
                make_montage(sequences[0], roll_folder, "AM_DB_Roll_%d_%s" % (index, direction))
            else:
                unreal.log_warning("DBFAB roll %s: no animation imported (%s)" % (direction, paths))

    # 2b. Fight pack: own (Mixamo-style) skeleton -> import, auto IK rigs, retarget onto the mannequin.
    fight_dir = os.path.join(SOURCE, "fight_mocap", "Fight Mocap Animation Data")
    fight_folder = ANIMS + "/Fight"
    if os.path.isdir(fight_dir):
        if library.does_directory_exist(fight_folder):
            library.delete_directory(fight_folder)
        mesh_paths = run_import(os.path.join(fight_dir, "Male_Lowpoly.fbx"), fight_folder + "/Source", "SK_Fight_Male", fbx_anim_options(None, True))
        source_mesh = next((unreal.load_asset(p) for p in mesh_paths if isinstance(unreal.load_asset(p), unreal.SkeletalMesh)), None)
        if source_mesh is None:
            unreal.log_error("DBFAB fight source mesh not imported: %s" % mesh_paths)
        else:
            source_skeleton = source_mesh.get_editor_property("skeleton")
            clips = {
                "05_04_007_attack_02.fbx": "Attack_Sword_A",
                "05_04_008_attack_03.fbx": "Attack_Sword_B",
                "05_01_017_dodge_B.fbx": "Dodge_Back",
                "05_01_018_dodge_R.fbx": "Dodge_Right",
                "05_01_019_dodge_L.fbx": "Dodge_Left",
                "05_02_010_Fight invite.fbx": "Taunt_Invite",
            }
            source_sequences = []
            for filename, name in clips.items():
                paths = run_import(os.path.join(fight_dir, filename), fight_folder + "/Source", "A_Fight_" + name, interchange_anim_options(source_skeleton))
                source_sequences += [a for a in (unreal.load_asset(p) for p in paths) if isinstance(a, unreal.AnimSequence)]
            log("fight source sequences: %d" % len(source_sequences))

            def make_rig(name, mesh):
                rig = tools.create_asset(name, fight_folder + "/Retarget", unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
                controller = unreal.IKRigController.get_controller(rig)
                controller.set_skeletal_mesh(mesh)
                controller.apply_auto_generated_retarget_definition()
                library.save_loaded_asset(rig, False)
                log("ik rig %s chains %d" % (name, len(controller.get_retarget_chains())))
                return rig

            source_rig = make_rig("IK_Fight_Male", source_mesh)
            target_rig = make_rig("IK_DB_Manny", manny_mesh)
            retargeter = tools.create_asset("RTG_Fight_To_Manny", fight_folder + "/Retarget", unreal.IKRetargeter, unreal.IKRetargetFactory())
            rtg = unreal.IKRetargeterController.get_controller(retargeter)
            rtg.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, source_rig)
            rtg.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, target_rig)
            try:
                rtg.add_default_ops()
                rtg.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE, source_rig)
                rtg.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, target_rig)
            except Exception as error:
                unreal.log_warning("DBFAB retarget ops: %s" % error)
            rtg.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
            try:
                rtg.auto_align_all_bones(unreal.RetargetSourceOrTarget.SOURCE)
            except Exception as error:
                unreal.log_warning("DBFAB auto align: %s" % error)
            library.save_loaded_asset(retargeter, False)

            retargeted = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
                [unreal.EditorAssetLibrary.find_asset_data(s.get_path_name()) for s in source_sequences],
                source_mesh, manny_mesh, retargeter, search="A_Fight_", replace="A_DB_Fight_", target_path=fight_folder,
                include_referenced_assets=False, overwrite_existing_files=True)
            log("retargeted assets: %d" % len(retargeted))
            for asset_data in retargeted:
                asset = asset_data.get_asset() if hasattr(asset_data, "get_asset") else asset_data
                if isinstance(asset, unreal.AnimSequence):
                    name = asset.get_name().replace("A_DB_Fight_", "")
                    make_montage(asset, fight_folder, "AM_DB_Fight_" + name)
    save_folder(ANIMS)

log("Fab import complete")


# ---- 3. DARK BLOOD materials for the models (consistent shading, and the glTF default material showed grey) --
mel = unreal.MaterialEditingLibrary


def kit_material(name, folder, master, textures, metallic=0.0, tint=(1.0, 1.0, 1.0)):
    base = unreal.load_asset(folder + "/" + textures[0])
    normal = unreal.load_asset(folder + "/" + textures[1])
    orm = unreal.load_asset(folder + "/" + textures[2]) if len(textures) > 2 else None
    parent = unreal.load_asset("/Game/DarkBlood/Art/Materials/Master/" + master)
    if base is None or parent is None:
        unreal.log_warning("DBFAB material %s: textures or master missing" % name)
        return None
    for texture, is_normal in ((normal, True), (orm, False)):
        if texture is None:
            continue
        texture.set_editor_property("srgb", False)
        texture.set_editor_property("compression_settings",
            unreal.TextureCompressionSettings.TC_NORMALMAP if is_normal else unreal.TextureCompressionSettings.TC_DEFAULT)
        library.save_loaded_asset(texture, False)
    path = folder + "/" + name
    instance = unreal.load_asset(path) or tools.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, parent)
    mel.set_material_instance_static_switch_parameter_value(instance, "UseTextures", True)
    mel.set_material_instance_static_switch_parameter_value(instance, "WorldAligned", False)
    mel.set_material_instance_texture_parameter_value(instance, "T_BaseColor", base)
    if normal is not None:
        mel.set_material_instance_texture_parameter_value(instance, "T_Normal", normal)
    if orm is not None:
        # glTF metallicRoughness: R unused (=1 -> AO 1), G roughness, B metal (metal comes from the scalar here).
        mel.set_material_instance_texture_parameter_value(instance, "T_ORM", orm)
    mel.set_material_instance_scalar_parameter_value(instance, "Metallic", metallic)
    mel.set_material_instance_scalar_parameter_value(instance, "MacroStrength", 0.2)
    mel.set_material_instance_vector_parameter_value(instance, "TextureTint", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
    mel.update_material_instance(instance)
    library.save_loaded_asset(instance, False)
    return instance


def assign(mesh_path, material, slot_name_part=None):
    mesh = unreal.load_asset(mesh_path)
    if mesh is None or material is None:
        return
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        if slot_name_part is None or slot_name_part in str(slot.get_editor_property("material_slot_name")):
            mesh.set_material(index, material)
    library.save_loaded_asset(mesh, False)
    log("material %s -> %s" % (material.get_name(), mesh.get_name()))


torii = ART + "/Torii_Pikas/scene"
gate = kit_material("MI_DB_Fab_Torii_Gate", torii + "/Textures", "M_DB_Wood_Master", ["Torri_gate_baseColor", "Torri_gate_normal", "Torri_gate_metallicRoughness"])
rope = kit_material("MI_DB_Fab_Torii_Rope", torii + "/Textures", "M_DB_Fabric_Master", ["RopeGold_baseColor", "RopeGold_normal", "RopeGold_metallicRoughness"])
assign(torii + "/StaticMeshes/Torri_Gate_Torri_gate_0", gate)
assign(torii + "/StaticMeshes/Torri_Gate_Rope_Gold_0", rope)
log("Fab materials complete")
