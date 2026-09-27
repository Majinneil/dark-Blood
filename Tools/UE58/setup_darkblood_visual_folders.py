# Run inside Unreal Editor Python environment (UE 5.8)
import unreal

FOLDERS = [
    "/Game/DarkBlood/Art/Environment/Landscape",
    "/Game/DarkBlood/Art/Environment/Rocks",
    "/Game/DarkBlood/Art/Environment/Foliage",
    "/Game/DarkBlood/Art/Environment/Water",
    "/Game/DarkBlood/Art/Environment/Roads",
    "/Game/DarkBlood/Art/Architecture/Capital",
    "/Game/DarkBlood/Art/Architecture/Villages",
    "/Game/DarkBlood/Art/Architecture/Temples",
    "/Game/DarkBlood/Art/Architecture/Shrines",
    "/Game/DarkBlood/Art/Architecture/Bridges",
    "/Game/DarkBlood/Art/Architecture/Props",
    "/Game/DarkBlood/Art/Materials/Master",
    "/Game/DarkBlood/Art/Materials/Landscape",
    "/Game/DarkBlood/Art/Materials/Architecture",
    "/Game/DarkBlood/Art/Materials/Organic",
    "/Game/DarkBlood/Art/Materials/DarkBlood",
    "/Game/DarkBlood/Art/Materials/Instances",
    "/Game/DarkBlood/Art/Architecture/Kits",
    "/Game/DarkBlood/Art/VFX",
    "/Game/DarkBlood/Art/Lighting",
    "/Game/DarkBlood/Art/PCG",
    "/Game/DarkBlood/Art/Dev",
    "/Game/DarkBlood/Characters/Profiles",
    "/Game/DarkBlood/Characters/Player",
    "/Game/DarkBlood/Characters/Heroes",
    "/Game/DarkBlood/Characters/NPC",
    "/Game/DarkBlood/Characters/Crowd",
    "/Game/DarkBlood/Characters/Clothing",
    "/Game/DarkBlood/Characters/Hair",
    "/Game/DarkBlood/Characters/Materials",
    "/Game/DarkBlood/Characters/Textures",
    "/Game/DarkBlood/Characters/Rigs",
    "/Game/DarkBlood/Animation/Sets",
    "/Game/DarkBlood/Animation/Locomotion",
    "/Game/DarkBlood/Animation/Combat",
    "/Game/DarkBlood/Animation/Facial",
    "/Game/DarkBlood/Animation/Mocap",
    "/Game/DarkBlood/VFX",
    "/Game/DarkBlood/Lighting",
    "/Game/DarkBlood/PCG",
    "/Game/DarkBlood/Dev/Visual",
]

for folder in FOLDERS:
    if not unreal.EditorAssetLibrary.does_directory_exist(folder):
        unreal.EditorAssetLibrary.make_directory(folder)
        unreal.log(f"Created: {folder}")
    else:
        unreal.log(f"Exists: {folder}")

unreal.log("DARK BLOOD visual folder setup complete.")
