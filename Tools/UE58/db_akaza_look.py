# Run in the Unreal Editor (UE 5.8, MetaHuman Creator):
#   UnrealEditor.exe DarkBlood.uproject "-ExecCmds=py <project>/Tools/UE58/db_akaza_look.py,QUIT_EDITOR"
#
# Brings Akaza (/Game/DarkBlood/Characters/MetaHumans/Akaza) to the project owner's reference sheet (docs/AKAZA_OUTFIT.md):
# full curly beard and curly moustache, dense brows, black hair (melanin), sun-tanned weathered skin, brown eyes.
# The hair stays pulled back (closest preset to the top knot); the curly top knot itself is a custom groom. After this
# run the character must be built again: db_metahuman_cast.py finish Akaza rebuild.
import unreal

TARGET = "/Game/DarkBlood/Characters/MetaHumans/Akaza"
GROOMS = "/MetaHumanCharacter/Optional/Grooms/Bindings/"
LOOK = {
    "Beard": "Beards/WI_Beard_M_Curly",
    "Mustache": "Mustaches/WI_Mustache_M_Curly",
    "Eyebrows": "Eyebrows/WI_Eyebrows_M_Dense",
}
EYE_PRESETS = "/MetaHumanCharacter/Tools/EyePresets/EyePresets"

library = unreal.EditorAssetLibrary
subsystem = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)


def log(message):
    unreal.log("DBLOOK " + message)


character = unreal.load_asset(TARGET)
if character is None:
    raise SystemExit("DBLOOK Akaza missing - run db_create_akaza_metahuman.py first")
collection = character.internal_collection
instance = collection.default_instance

# Grooms: swap the item in each slot (the selection of a slot holds one item).
for slot, relative in LOOK.items():
    item = unreal.load_asset(GROOMS + relative)
    if item is None:
        unreal.log_warning("DBLOOK groom missing: " + relative)
        continue
    for selection in list(instance.get_editor_property("slot_selections")):
        if str(selection.get_editor_property("slot_name")) == slot:
            instance.try_remove_slot_selection(selection)
    key = collection.try_add_item_from_wardrobe_item(slot, item)
    instance.try_add_slot_selection(unreal.MetaHumanPipelineSlotSelection(slot_name=slot, selected_item=key))
    log("%s: %s" % (slot, relative.split("/")[-1]))

added = subsystem.try_add_object_to_edit(character)
try:
    subsystem.assemble_for_preview(character=character)
    # Black hair and beard.
    for selection in instance.get_editor_property("slot_selections"):
        key = selection.get_editor_property("selected_item")
        for parameter in instance.get_instance_parameters(item_path=unreal.MetaHumanPaletteItemPath(item_key=key)):
            if parameter.name == "Melanin":
                parameter.set_float(value=0.95)
    # Skin: sun-tanned (U is the skin's depth - darker), warm undertone, a little rougher - weathered.
    settings = character.get_editor_property("skin_settings")
    skin = settings.get_editor_property("skin")
    skin.set_editor_property("u", 0.62)
    skin.set_editor_property("v", 0.47)
    skin.set_editor_property("roughness", 1.1)
    settings.set_editor_property("skin", skin)
    subsystem.commit_skin_settings(character, settings)
    log("skin: tanned (u 0.62, v 0.47)")
    # Eyes: a brown preset from Epic's eye presets, else a dark brown by hand.
    eyes = character.get_editor_property("eyes_settings")
    brown = None
    presets = unreal.load_asset(EYE_PRESETS)
    if presets is not None:
        for name in ("presets", "eye_presets", "Presets"):
            try:
                entries = presets.get_editor_property(name)
            except Exception:
                continue
            for entry in entries:
                label = str(entry.get_editor_property("preset_name")) if hasattr(entry, "preset_name") else str(entry)
                if "brown" in label.lower():
                    brown = entry.get_editor_property("eyes_settings")
                    log("eyes: preset " + label)
                    break
            break
    if brown is None:
        for side in ("eye_left", "eye_right"):
            eye = eyes.get_editor_property(side)
            iris = eye.get_editor_property("iris")
            iris.set_editor_property("primary_color_u", 0.08)
            iris.set_editor_property("primary_color_v", 0.35)
            iris.set_editor_property("secondary_color_u", 0.07)
            iris.set_editor_property("secondary_color_v", 0.2)
            eye.set_editor_property("iris", iris)
            eyes.set_editor_property(side, eye)
        brown = eyes
        log("eyes: dark brown by hand")
    subsystem.commit_eyes_settings(character, brown)
finally:
    if added and subsystem.is_object_added_for_editing(character):
        subsystem.remove_object_to_edit(character)
library.save_loaded_asset(character, False)
log("Akaza look saved - build again: db_metahuman_cast.py finish Akaza rebuild")
