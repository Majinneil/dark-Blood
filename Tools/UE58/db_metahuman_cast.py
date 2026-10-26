# Run in the Unreal Editor console (UE 5.8, MetaHuman Creator):
#   py "Tools/UE58/db_metahuman_cast.py" create          create the cast's character assets from Epic presets (offline)
#   py "Tools/UE58/db_metahuman_cast.py" finish [Name]   cloud auto rig (joints only), texture sources and optimized build of every cast
#                                                        member (or one), skipping those already built - needs the
#                                                        Epic account authorization of MetaHuman Creator once
#
# The cast of DARK BLOOD as MetaHumans: the hero Akaza (db_create_akaza_metahuman.py) and the human NPCs. Characters
# live in /Game/DarkBlood/Characters/MetaHumans/<Name>, builds in .../MetaHumans/Build/<Name> (blueprint BP_<Name>);
# the game registers each build as visual profile CV_MH_<Name> (DBDevelopmentContent) and uses it where it exists.
import sys

import unreal

ROOT = "/Game/DarkBlood/Characters/MetaHumans"
BUILD = ROOT + "/Build"
PRESETS = "/MetaHumanCharacter/Optional/Presets/"
# Name -> Epic preset. Roles: NPC_King, NPC_Captain (named NPCs), Villager_* (settlement life picks by index).
CAST = {
    "NPC_King": "Walter",
    "NPC_Captain": "Kelvin",
    "Villager_01": "Bo",
    "Villager_02": "Aera",
    "Villager_03": "Aoi",
    "Villager_04": "Sook-ja",
    "Villager_05": "Tuya",
    "Villager_06": "Mateo",
}
library = unreal.EditorAssetLibrary
subsystem = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)


def log(message):
    unreal.log("DBCAST " + message)


def create():
    for name, preset in CAST.items():
        path = ROOT + "/" + name
        if library.does_asset_exist(path):
            log("%s exists" % name)
            continue
        if library.duplicate_asset(PRESETS + preset, path):
            log("%s created from %s" % (name, preset))
        else:
            unreal.log_warning("DBCAST %s: preset %s missing" % (name, preset))


def set_texture_resolutions(character, hero):
    """Returns True if the resolutions changed (the texture sources must then be fetched again)."""
    skin = character.get_editor_property("skin_settings")
    wanted = skin.get_editor_property("desired_texture_sources_resolutions")
    face = unreal.RequestTextureResolution.RES4K if hero else unreal.RequestTextureResolution.RES2K
    body = unreal.RequestTextureResolution.RES2K
    values = {"face_albedo": face, "face_normal": face, "face_cavity": face, "face_animated_maps": body,
              "body_albedo": body, "body_normal": body, "body_cavity": body, "body_masks": body}
    changed = False
    for field, value in values.items():
        if wanted.get_editor_property(field) != value:
            wanted.set_editor_property(field, value)
            changed = True
    if changed:
        skin.set_editor_property("desired_texture_sources_resolutions", wanted)
        subsystem.commit_skin_settings(character, skin)
    return changed


def finish(only=None):
    names = ["Akaza"] + list(CAST)
    for name in names:
        if only and name != only:
            continue
        if library.does_asset_exist("%s/%s/BP_%s" % (BUILD, name, name)):
            log("%s already built" % name)
            continue
        character = unreal.load_asset(ROOT + "/" + name)
        if character is None:
            continue
        added = False
        if not subsystem.is_object_added_for_editing(character):
            added = subsystem.try_add_object_to_edit(character)
        try:
            if not subsystem.can_build_meta_human(character, False):
                request = unreal.MetaHumanCharacterAutoRiggingRequestParams()
                request.blocking = True
                request.report_progress = False
                request.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY
                subsystem.request_auto_rigging(character, request)
                # Keep the cloud result at once, whatever happens next.
                library.save_loaded_asset(character, False)
                log("%s rigged" % name)
            # Texture sources sized for a game: the hero's face 4K, everything else 2K. 8K sources made the build
            # need more than 90 GB of memory (it compresses many 8K maps at once) and cost video memory in play.
            resized = set_texture_resolutions(character, name == "Akaza")
            if resized:
                log("%s texture sources set to %s" % (name, "4K face / 2K body" if name == "Akaza" else "2K"))
            if resized or not subsystem.can_build_meta_human(character, False):
                # The build also needs the face and body texture sources (Epic cloud, same authorization).
                textures = unreal.MetaHumanCharacterTextureRequestParams()
                textures.blocking = True
                textures.report_progress = False
                subsystem.request_texture_sources(character, textures)
                library.save_loaded_asset(character, False)
                log("%s textures downloaded" % name)
            if not subsystem.can_build_meta_human(character, True):
                unreal.log_warning("DBCAST %s: not buildable (rig or textures missing - see the MetaHuman error above)" % name)
                continue
            params = unreal.MetaHumanCharacterEditorBuildParameters()
            params.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
            params.pipeline_quality = unreal.MetaHumanQualityLevel.MEDIUM
            params.absolute_build_path = BUILD + "/" + name
            params.common_folder_path = BUILD + "/Common"
            params.name_override = name
            params.enable_wardrobe_item_validation = False
            subsystem.build_meta_human(character=character, params=params)
            library.save_loaded_asset(character, False)
            log("%s built" % name)
        finally:
            if added and subsystem.is_object_added_for_editing(character):
                subsystem.remove_object_to_edit(character)


mode = sys.argv[1] if len(sys.argv) > 1 else "create"
if mode == "create":
    create()
elif mode == "finish":
    finish(sys.argv[2] if len(sys.argv) > 2 else None)
