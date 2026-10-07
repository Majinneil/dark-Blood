# Run inside the Unreal Editor (UE 5.8, MetaHuman Creator plugin + MetaHuman Creator Core Data installed):
#   in the editor console: py "Tools/UE58/db_create_akaza_metahuman.py" [Preset] [fresh]
# Preset picks the starting face and body from /MetaHumanCharacter/Optional/Presets (default Bruce); "fresh" replaces
# an existing Akaza asset. (-ExecutePythonScript on the command line closes the editor afterwards in UE 5.8.)
#
# Starts the hero Akaza Kurosaki as a MetaHuman character asset (/Game/DarkBlood/Characters/MetaHumans/Akaza):
# a copy of a preset face, then the grooms of his reference sheet (SourceArt/Hyper3D/Akaza/input) - black hair tied
# back, short full beard and moustache, thick brows - and a warm medium skin. The face is refined by hand in the
# MetaHuman Creator, which this script opens; auto rigging and assembly follow there (cloud services, Epic login).
import sys

import unreal

TARGET_FOLDER = "/Game/DarkBlood/Characters/MetaHumans"
TARGET = TARGET_FOLDER + "/Akaza"
PRESETS = "/MetaHumanCharacter/Optional/Presets/"
GROOMS = "/MetaHumanCharacter/Optional/Grooms/Bindings/"
LOOK = [
    ("Hair", "Hair/WI_Hair_S_Updo"),
    ("Beard", "Beards/WI_Beard_S_Full"),
    ("Mustache", "Mustaches/WI_Mustache_S_Full"),
    ("Eyebrows", "Eyebrows/WI_Eyebrows_M_Thick"),
    ("Eyelashes", "Eyelashes/WI_Eyelashes_S_Fine"),
    ("Peachfuzz", "Peachfuzz/WI_Peachfuzz_M_Thin"),
]

library = unreal.EditorAssetLibrary


def log(message):
    unreal.log("DBAKAZA " + message)


preset = sys.argv[1] if len(sys.argv) > 1 else "Bruce"
if "fresh" in sys.argv[2:] and library.does_asset_exist(TARGET):
    old = unreal.load_asset(TARGET)
    unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).close_all_editors_for_asset(old)
    old_subsystem = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    if old_subsystem.is_object_added_for_editing(old):
        old_subsystem.remove_object_to_edit(old)
    del old
    library.delete_asset(TARGET)
    log("old character removed")

if library.does_asset_exist(TARGET):
    character = unreal.load_asset(TARGET)
    log("existing character kept: " + TARGET)
else:
    character = library.duplicate_asset(PRESETS + preset, TARGET)
    log("created %s from preset %s" % (TARGET, preset))
if character is None:
    raise RuntimeError("could not create " + TARGET)

collection = character.internal_collection
keys = []
for slot, relative in LOOK:
    item = unreal.load_asset(GROOMS + relative)
    if item is None:
        unreal.log_warning("DBAKAZA groom missing: " + relative)
        continue
    key = collection.try_add_item_from_wardrobe_item(slot, item)
    selection = unreal.MetaHumanPipelineSlotSelection(slot_name=slot, selected_item=key)
    collection.default_instance.try_add_slot_selection(selection)
    keys.append(key)
    log("%s: %s" % (slot, relative.split("/")[-1]))

subsystem = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if subsystem.try_add_object_to_edit(character):
    try:
        subsystem.assemble_for_preview(character=character)
        # Black hair: high melanin on every groom.
        for key in keys:
            parameters = collection.default_instance.get_instance_parameters(item_path=unreal.MetaHumanPaletteItemPath(item_key=key))
            for parameter in parameters:
                if parameter.name == "Melanin":
                    parameter.set_float(value=0.95)
        skin = unreal.MetaHumanCharacterSkinProperties()
        skin.u = 0.52
        skin.v = 0.42
        skin.roughness = 0.8
        settings = unreal.MetaHumanCharacterSkinSettings()
        settings.skin = skin
        subsystem.commit_skin_settings(character, settings)
        log("skin committed (warm medium)")
    finally:
        if subsystem.is_object_added_for_editing(character):
            subsystem.remove_object_to_edit(character)
library.save_loaded_asset(character, False)
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets(assets=[character])
log("opened in MetaHuman Creator")
