# Run inside the Unreal Editor Python environment (UE 5.8):
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_fix_material_usage.py
#
# Sets the usage flags "Nanite" and "Instanced Static Meshes" on every base material of the project. Imported materials
# (glTF / FBX) come without them; in a game world the engine then cannot compile the missing permutation and draws the
# mesh the slow classic way (or with the default material). Re-run after importing new models.
import unreal

library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
FLAGS = ("used_with_nanite", "used_with_instanced_static_meshes")

checked = 0
fixed = 0
outside = set()
for path in library.list_assets("/Game/DarkBlood", recursive=True, include_folder=False):
    if not path.split(".")[0].split("/")[-1]:
        continue
    data = library.find_asset_data(path)
    class_name = str(data.get_editor_property("asset_class_path").get_editor_property("asset_name"))
    if class_name not in ("Material", "MaterialInstanceConstant"):
        continue
    asset = unreal.load_asset(path)
    base = asset.get_base_material() if class_name == "MaterialInstanceConstant" else asset
    if base is None:
        continue
    base_path = base.get_path_name()
    if not base_path.startswith("/Game/"):
        outside.add(base_path)
        continue
    checked += 1
    changed = False
    for flag in FLAGS:
        if not base.get_editor_property(flag):
            base.set_editor_property(flag, True)
            changed = True
    if changed:
        mel.recompile_material(base)
        library.save_loaded_asset(base, False)
        fixed += 1
        unreal.log("DBUSAGE fixed " + base_path)

# Imported instances whose base lives in the Interchange plugin (glTF M_Default, FBX Phong, ...): copy the plugin parents
# into the project once (with the flags) and re-parent the instances there - plugin content stays untouched.
IMPORTED = "/Game/DarkBlood/Art/Materials/Imported"
tools = unreal.AssetToolsHelpers.get_asset_tools()
copies = {}


def project_copy(original):
    source = original.get_path_name().split(".")[0]
    if source in copies:
        return copies[source]
    name = "DB_" + source.split("/")[-1]
    copy = unreal.load_asset(IMPORTED + "/" + name)
    if copy is None:
        copy = tools.duplicate_asset(name, IMPORTED, original)
    if isinstance(copy, unreal.MaterialInstanceConstant):
        parent = copy.get_editor_property("parent")
        if parent is not None and not parent.get_path_name().startswith("/Game/"):
            mel.set_material_instance_parent(copy, project_copy(parent))
    else:
        for flag in FLAGS:
            copy.set_editor_property(flag, True)
        mel.recompile_material(copy)
    library.save_loaded_asset(copy, False)
    copies[source] = copy
    unreal.log("DBUSAGE project copy " + copy.get_path_name())
    return copy


reparented = 0
for path in library.list_assets("/Game/DarkBlood", recursive=True, include_folder=False):
    data = library.find_asset_data(path)
    if str(data.get_editor_property("asset_class_path").get_editor_property("asset_name")) != "MaterialInstanceConstant":
        continue
    if path.startswith(IMPORTED):
        continue
    instance = unreal.load_asset(path)
    parent = instance.get_editor_property("parent")
    if parent is None or parent.get_path_name().startswith("/Game/"):
        continue
    mel.set_material_instance_parent(instance, project_copy(parent))
    mel.update_material_instance(instance)
    library.save_loaded_asset(instance, False)
    reparented += 1
unreal.log("DBUSAGE re-parented %d imported instances to project copies" % reparented)

unreal.log("DBUSAGE checked %d materials, fixed %d; base materials outside /Game: %s" % (checked, fixed, ", ".join(sorted(outside)) or "none"))
