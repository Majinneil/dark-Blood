# Run in the Unreal Editor (UE 5.8, MetaHuman Creator), e.g. from the console:
#   py "Tools/UE58/db_conform_metahuman_head.py" Akaza
#
# Fits the face of the MetaHuman character /Game/DarkBlood/Characters/MetaHumans/<Name> to the head of its generated
# model: SourceArt/Hyper3D/<Name>/metahuman/head_target.json + head_portrait.png from
# Tools/UE58/blender_metahuman_head_target.py. Head only (HEAD_ONLY) - clothing never shapes the body. The face
# landmarks tracked on the portrait guide the solve; grooms, skin and body of the character stay as they are.
import json
import os
import sys

import unreal

NAME = sys.argv[1] if len(sys.argv) > 1 else "Akaza"
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
FOLDER = os.path.join(PROJECT, "SourceArt", "Hyper3D", NAME, "metahuman")
ASSET = "/Game/DarkBlood/Characters/MetaHumans/" + NAME


def log(message):
    unreal.log("DBCONFORM " + message)


with open(os.path.join(FOLDER, "head_target.json")) as handle:
    target = json.load(handle)
portrait = os.path.join(FOLDER, "head_portrait.png")

subsystem = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
image_size, pixels = unreal.PromotedFrameUtils.get_promoted_frame_as_pixel_array_from_disk(portrait)
tracked = subsystem.track_face_landmarks_from_image(pixels, image_size.x, image_size.y)
if isinstance(tracked, tuple) and len(tracked) == 1:
    tracked = tracked[0]
if not tracked:
    raise RuntimeError("no face landmarks found on " + portrait)
log("%d landmark curves on the portrait" % len(tracked))

character = unreal.load_asset(ASSET)
if character is None:
    raise RuntimeError("missing " + ASSET)
added_here = False
if not subsystem.is_object_added_for_editing(character):
    if not subsystem.try_add_object_to_edit(character):
        raise RuntimeError("cannot edit " + ASSET)
    added_here = True
try:
    params = unreal.ConformTargetParams()
    mesh = params.conform_target_mesh
    mesh.target_parts_type = unreal.TargetPartsType.HEAD_ONLY
    mesh.head_vertices = [unreal.Vector3f(*v) for v in target["vertices"]]
    mesh.head_vertex_indices = target["triangles"]
    params.conform_target_mesh = mesh
    params.auto_solve = True
    # Solver presets: Body/IdentityTemplate/pipeline_presets.json of the MetaHumanCharacter plugin.
    settings = params.body_conform_solve_settings
    settings.pipeline_name = "head_only"
    params.body_conform_solve_settings = settings
    params.estimate_body_joints_from_mesh = False
    camera = target["camera"]
    view = unreal.MinimalViewInfo()
    view.location = unreal.Vector(*camera["location"])
    view.rotation = unreal.Rotator(pitch=0.0, yaw=camera["yaw"], roll=0.0)
    view.fov = camera["fov"]
    view.aspect_ratio = float(camera["width"]) / float(camera["height"])
    view.projection_mode = unreal.CameraProjectionMode.PERSPECTIVE
    params.curve_tracking_points = tracked
    params.camera_view_info = view
    params.image_size = image_size

    key = unreal.MetaHumanCharacterTargetMeshKey()
    # First a rigid alignment (scale, rotation, translation) of the character onto the target head, then the solve.
    if not subsystem.align_to_target_meshes(character, key, params):
        raise RuntimeError("align_to_target_meshes failed")
    log("aligned to the target head")
    if not subsystem.conform_to_target_meshes(character, key, params):
        raise RuntimeError("conform_to_target_meshes failed")
    subsystem.commit_face_state(character)
    unreal.EditorAssetLibrary.save_loaded_asset(character, False)
    log("face of %s conformed to %d target vertices" % (ASSET, len(target["vertices"])))
finally:
    if added_here and subsystem.is_object_added_for_editing(character):
        subsystem.remove_object_to_edit(character)
