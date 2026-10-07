# Run in the Unreal Editor console (UE 5.8, MetaHuman Creator, Epic account signed in for the cloud services):
#   py "Tools/UE58/db_finish_akaza_metahuman.py" [look] [rig] [textures] [build]   (no argument: all steps)
#
# Finishes the hero Akaza Kurosaki (/Game/DarkBlood/Characters/MetaHumans/Akaza, from db_create_akaza_metahuman.py):
#   look      grooms of his sheet - hair tied up, short full beard and moustache, thick brows (single selection per slot)
#   rig       cloud auto rigging (joints only: cheap at runtime, the face still animates through the joints)
#   textures  high resolution skin texture sources (cloud texture synthesis)
#   build     optimized game assembly (medium quality, hair as cards) into /Game/DarkBlood/Characters/MetaHumans/Build
import sys

import unreal

ASSET = "/Game/DarkBlood/Characters/MetaHumans/Akaza"
BUILD = "/Game/DarkBlood/Characters/MetaHumans/Build"
GROOMS = "/MetaHumanCharacter/Optional/Grooms/Bindings/"
LOOK = {
    "Hair": "Hair/WI_Hair_S_UpdoBuns",
    "Beard": "Beards/WI_Beard_S_Full",
    "Mustache": "Mustaches/WI_Mustache_S_Full",
    "Eyebrows": "Eyebrows/WI_Eyebrows_M_Thick",
}
steps = [a for a in sys.argv[1:] if a in ("look", "rig", "textures", "build")] or ["look", "rig", "textures", "build"]


def log(message):
    unreal.log("DBFINISH " + message)


character = unreal.load_asset(ASSET)
subsystem = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
added_here = False
if not subsystem.is_object_added_for_editing(character):
    if not subsystem.try_add_object_to_edit(character):
        raise RuntimeError("cannot edit " + ASSET)
    added_here = True
try:
    if "look" in steps:
        collection = character.internal_collection
        for slot, relative in LOOK.items():
            item = unreal.load_asset(GROOMS + relative)
            if item is None:
                unreal.log_warning("DBFINISH missing groom " + relative)
                continue
            keys = list(collection.get_item_keys_for_wardrobe_item(item)) if hasattr(collection, "get_item_keys_for_wardrobe_item") else []
            key = keys[0] if keys else collection.try_add_item_from_wardrobe_item(slot, item)
            collection.default_instance.set_single_slot_selection(slot, key)
            log("%s -> %s" % (slot, relative.split("/")[-1]))
        subsystem.assemble_for_preview(character=character)
        log("look assembled")

    if "rig" in steps:
        request = unreal.MetaHumanCharacterAutoRiggingRequestParams()
        request.blocking = True
        request.report_progress = False
        request.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY
        subsystem.request_auto_rigging(character, request)
        log("auto rigging requested (blocking) - done")

    if "textures" in steps:
        textures = unreal.MetaHumanCharacterTextureRequestParams()
        for name in ("blocking", "report_progress"):
            try:
                textures.set_editor_property(name, name == "blocking")
            except Exception:
                pass
        subsystem.request_texture_sources(character, textures)
        log("high resolution texture sources requested, has_high_resolution_textures=%s" % character.has_high_resolution_textures)

    if "build" in steps:
        if not subsystem.can_build_meta_human(character, True):
            raise RuntimeError("character not ready for assembly (see log)")
        params = unreal.MetaHumanCharacterEditorBuildParameters()
        params.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
        params.pipeline_quality = unreal.MetaHumanQualityLevel.MEDIUM
        params.absolute_build_path = BUILD
        params.common_folder_path = BUILD + "/Common"
        params.enable_wardrobe_item_validation = False
        subsystem.build_meta_human(character=character, params=params)
        log("built into " + BUILD)
    unreal.EditorAssetLibrary.save_loaded_asset(character, False)
finally:
    if added_here and subsystem.is_object_added_for_editing(character):
        subsystem.remove_object_to_edit(character)
