# Run inside the Unreal Editor Python environment (UE 5.8) after db_import_sketchfab.py and db_fix_material_usage.py:
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_fix_sketchfab_materials.py
#
# Some Sketchfab models draw thatch and foliage as alpha cards but arrive with opaque materials (the cards then show
# white where they should be see-through). Their instances are switched to the two-sided masked parent.
import unreal

ROOT = "/Game/DarkBlood/Art/Environment/Sketchfab"
MASKED = "/Game/DarkBlood/Art/Materials/Imported/DB_MI_Default_Mask_DS"
# model folder: material instance names that are alpha cards
CARDS = {"shirakawago_house": ["M_Shirakawago_House_1_1", "M_Shirakawago_House_1_2", "M_Shirakawago_House_1_3"]}

library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
masked = unreal.load_asset(MASKED)
for key, names in CARDS.items():
    for path in library.list_assets(ROOT + "/" + key, recursive=True, include_folder=False):
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.MaterialInstanceConstant) and asset.get_name() in names and masked:
            mel.set_material_instance_parent(asset, masked)
            mel.update_material_instance(asset)
            library.save_loaded_asset(asset, False)
            unreal.log("DBSFMAT masked " + asset.get_name())
