# Run in the Unreal Editor console (UE 5.8, MetaHuman Creator):
#   py "Tools/UE58/db_metahuman_cast.py" create          create the cast's character assets from Epic presets (offline)
#   py "Tools/UE58/db_metahuman_cast.py" finish [Name]   cloud auto rig (joints only), texture sources and optimized build of every cast
#                                                        member (or one), skipping those already built - needs the
#                                                        Epic account authorization of MetaHuman Creator once
#   py "Tools/UE58/db_metahuman_cast.py" finish Name rebuild   build that character again (after changing its look)
#   py "Tools/UE58/db_metahuman_cast.py" colour Name     set the hair colour on an existing build
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
# Hair colour of a build (melanin, redness): the creator's colour parameters only exist in a finished preview assembly, so
# it goes into the materials the build wrote. Akaza's hair and beard are black.
HAIR_COLOUR = {"Akaza": (0.98, 0.05)}
library = unreal.EditorAssetLibrary
subsystem = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)


def log(message):
    unreal.log("DBCAST " + message)


def blueprint_path(name):
    # The pipeline puts everything of a character into a folder of its name inside the build path.
    return "%s/%s/%s/BP_%s" % (BUILD, name, name, name)


def apply_hair_colour(name):
    """Sets melanin and redness on every material of the build's grooms and on the face (baked hair of the far LODs)."""
    if name not in HAIR_COLOUR:
        return
    melanin, redness = HAIR_COLOUR[name]
    material_library = unreal.MaterialEditingLibrary
    folder = "%s/%s/%s" % (BUILD, name, name)
    for path in library.list_assets(folder, recursive=True, include_folder=False):
        if "/Grooms/" not in path and "/Face/Materials/" not in path:
            continue
        material = unreal.load_asset(path)
        if not isinstance(material, unreal.MaterialInstanceConstant):
            continue
        changed = []
        for parameter in material_library.get_scalar_parameter_names(material):
            lower = str(parameter).lower()
            value = melanin if lower.endswith("melanin") else redness if lower.endswith("redness") else None
            if value is not None and not lower.startswith("region"):
                material_library.set_material_instance_scalar_parameter_value(material, parameter, value)
                changed.append(str(parameter))
        if changed:
            library.save_loaded_asset(material, False)
            log("%s: %s" % (material.get_name(), ", ".join(changed)))


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
    # The hero 4K throughout (seen close in every scene), NPCs 2K (many on screen at once: video memory).
    face = body = unreal.RequestTextureResolution.RES4K if hero else unreal.RequestTextureResolution.RES2K
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


def finish(only=None, rebuild=False):
    names = ["Akaza"] + list(CAST)
    for name in names:
        if only and name != only:
            continue
        if not rebuild and library.does_asset_exist(blueprint_path(name)):
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
                log("%s texture sources set to %s" % (name, "4K" if name == "Akaza" else "2K"))
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
            try:
                subsystem.build_meta_human(character=character, params=params)
            except RuntimeError as error:
                # Python turns every error line the build logged into an exception, also when the build went through
                # (the control rigs report "Cannot break link" while they are copied): only the blueprint decides.
                unreal.log_warning("DBCAST %s: build reported: %s" % (name, str(error)[:400]))
            if not library.does_asset_exist(blueprint_path(name)):
                unreal.log_error("DBCAST %s: build failed, no %s" % (name, blueprint_path(name)))
                continue
            # The build leaves its assets unsaved in memory. Saving reports a mismatch of the ARKit mapping's skeleton
            # as an error too, after everything has been written.
            try:
                library.save_directory(BUILD, only_if_is_dirty=True, recursive=True)
            except RuntimeError as error:
                unreal.log_warning("DBCAST %s: saving reported: %s" % (name, str(error)[:400]))
            library.save_loaded_asset(character, False)
            apply_hair_colour(name)
            log("%s built" % name)
        finally:
            if added and subsystem.is_object_added_for_editing(character):
                subsystem.remove_object_to_edit(character)


mode = sys.argv[1] if len(sys.argv) > 1 else "create"
if mode == "create":
    create()
elif mode == "colour":
    apply_hair_colour(sys.argv[2] if len(sys.argv) > 2 else "Akaza")
elif mode == "finish":
    finish(sys.argv[2] if len(sys.argv) > 2 else None, len(sys.argv) > 3 and sys.argv[3] == "rebuild")
