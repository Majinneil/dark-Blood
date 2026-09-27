# Run inside the Unreal Editor Python environment (UE 5.8), usually via Setup-DevMannequin.ps1.
# Builds the DEV montages referenced by the code-registered visual profiles (DBDevelopmentContent.cpp).
# Sources are the template mannequin animations copied into /Game/Characters/Mannequins (not committed).
# Gameplay moves characters in code, so every source animation is root-locked (in place).
import unreal

TARGET = "/Game/DarkBlood/Dev/Mannequin"
ANIMS = "/Game/Characters/Mannequins/Anims"

# (montage name, source sequence, auto blend out)
MONTAGES = [
    ("AM_DB_Dev_Attack_01", ANIMS + "/Unarmed/Attack/MM_Attack_01", True),
    ("AM_DB_Dev_Attack_02", ANIMS + "/Unarmed/Attack/MM_Attack_02", True),
    ("AM_DB_Dev_Attack_03", ANIMS + "/Unarmed/Attack/MM_Attack_03", True),
    ("AM_DB_Dev_Attack_Heavy", ANIMS + "/Unarmed/Attack/MM_ChargedAttack", True),
    ("AM_DB_Dev_Dodge", ANIMS + "/Unarmed/Jump/MM_Dash", True),
    ("AM_DB_Dev_HitReact", ANIMS + "/Rifle/HitReact/MM_HitReact_Front_Lgt_01", True),
    ("AM_DB_Dev_HitReact_Heavy", ANIMS + "/Rifle/HitReact/MM_HitReact_Front_Hvy_01", True),
    ("AM_DB_Dev_Death", ANIMS + "/Death/MM_Death_Front_01", False),
    ("AM_DB_Dev_Death_Back", ANIMS + "/Death/MM_Death_Back_01", False),
]

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary

if not library.does_directory_exist(TARGET):
    library.make_directory(TARGET)

for name, source, auto_blend_out in MONTAGES:
    sequence = unreal.load_asset(source)
    if not sequence:
        unreal.log_error("DBSETUP missing source animation: " + source)
        continue
    sequence.set_editor_property("force_root_lock", True)
    library.save_loaded_asset(sequence, False)

    path = TARGET + "/" + name
    if library.does_asset_exist(path):
        library.delete_asset(path)
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property("source_animation", sequence)
    montage = tools.create_asset(name, TARGET, unreal.AnimMontage, factory)
    if not montage:
        unreal.log_error("DBSETUP could not create " + path)
        continue
    montage.set_editor_property("enable_auto_blend_out", auto_blend_out)
    library.save_loaded_asset(montage, False)
    unreal.log("DBSETUP created " + path)

unreal.log("DBSETUP dev animations complete")
