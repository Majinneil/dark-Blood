# Run inside the Unreal Editor Python environment (UE 5.8):
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_create_ship_materials.py
#
# Sail cloth for the ships (DBShipArt): MI_DB_Fabric_Black, the crimson fabric instance re-tinted to a charcoal black
# (the fighting ship's sails and the war fleet's banners).
import unreal

INSTANCES = "/Game/DarkBlood/Art/Materials/Instances"
library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

path = INSTANCES + "/MI_DB_Fabric_Black"
if not library.does_asset_exist(path):
    library.duplicate_asset(INSTANCES + "/MI_DB_Fabric_Crimson", path)
instance = unreal.load_asset(path)
for name in mel.get_vector_parameter_names(instance):
    if "Tint" in str(name) or "Color" in str(name):
        mel.set_material_instance_vector_parameter_value(instance, name, unreal.LinearColor(0.035, 0.03, 0.03, 1.0))
        unreal.log("DBSHIPMAT %s -> black" % name)
mel.update_material_instance(instance)
library.save_loaded_asset(instance, False)
unreal.log("DBSHIPMAT MI_DB_Fabric_Black")
