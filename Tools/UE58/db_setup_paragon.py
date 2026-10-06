# Run inside the Unreal Editor Python environment (UE 5.8):
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_setup_paragon.py
#
# Prepares the free Epic "Paragon" characters (Fab, added to the project through the Epic Games Launcher into
# /Game/Paragon<Hero>) for DARK BLOOD: finds each hero's body mesh, its idle and jog loops and builds root-locked
# montages for our animation keys (attacks, hit reaction, knockdown, death, signature) in
# /Game/DarkBlood/Characters/Paragon/<Hero>. Each hero keeps its own skeleton and moves; the game drives them with
# UDBNativeLocomotionAnimInstance (no Animation Blueprint). Prints a manifest line per hero (DBPARAGON ...) with the
# paths and the mesh height, which DBDevelopmentContent.cpp registers as visual profiles. Re-running rebuilds.
import os
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
TARGET = "/Game/DarkBlood/Characters/Paragon"
library = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# Montage name -> candidate source animations (first found wins).
MONTAGES = [
    ("Attack_01", ["Primary_Attack_A_Normal", "Primary_Attack_A_Fast_V1", "Primary_Attack_A", "Primary_Attack_Normal", "Attack_Melee_A", "Attack_A"]),
    ("Attack_02", ["Primary_Attack_B_Normal", "Primary_Attack_B_Fast_V1", "Primary_Attack_B", "Primary_Attack_Normal", "Attack_Melee_B", "Attack_B"]),
    ("Attack_03", ["Primary_Attack_C_Normal", "Primary_Attack_Normal", "Primary_Attack_Slow", "Attack_Melee_C", "Attack_C", "Primary_Attack_A_Slow"]),
    ("Attack_Heavy", ["Ability_RMB", "Primary_Attack_Slow", "Primary_Attack_A_Slow", "Ability_Q"]),
    ("Signature", ["Ability_Ultimate", "Ability_R", "Ability_E", "Ability_Q"]),
    ("HitReact", ["Hitreact_Fwd", "HitReact_Front", "Hit_React_Fwd", "HitReact_Fwd"]),
    ("Knockdown", ["Knock_Fwd", "Knockback_Fwd", "Stun_Start", "Hitreact_Bwd"]),
    ("Death", ["Death", "Death_A", "Death_Fwd", "Death_01"]),
    ("Spawn", ["LevelStart", "Spawn", "Emote_Taunt"]),
]
IDLE = ["Idle_Relaxed", "Idle", "Idle_NonAdditive", "Idle_Straight", "Idle_Pose", "Idle_Combat"]
RUN = ["Jog_Fwd", "Jog_Fwd_Combat", "Run_Fwd", "Walk_Fwd"]


def log(message):
    unreal.log("DBPARAGON " + message)


def asset_names(folder):
    names = {}
    for path in library.list_assets(folder, recursive=True, include_folder=False):
        names.setdefault(path.split(".")[0].split("/")[-1], path.split(".")[0])
    return names


def is_base_pose(path):
    # Additive clips (Khaimera's "Idle" belongs to Idle_Zero_Pose) collapse the skeleton when played on their own.
    sequence = unreal.load_asset(path)
    if not isinstance(sequence, unreal.AnimSequence):
        return True
    return sequence.get_editor_property("additive_anim_type") == unreal.AdditiveAnimationType.AAT_NONE


def pick(names, candidates, suffix_ok=True):
    for candidate in candidates:
        if candidate in names and is_base_pose(names[candidate]):
            return names[candidate]
    return None


# Fallback patterns when a hero names its clips differently (Greystone: Attack_A_Med, Attack_PrimaryA ...).
PATTERNS = {
    # Greystone, Grux (left/right arm), Khaimera (Melee_A..), Kwang/Morigesh (PrimaryAttack_A_Slow), Sevarog (Swing1..),
    # minions (Attack_A..)
    "Attack_01": ["Attack_PrimaryA", "Attack_A_Med", "Attack_A_Fast", "PrimaryAttack_LA", "Melee_A", "PrimaryAttack_A_Slow", "Swing1_Medium",
                  "PrimaryAttack_A", "Primary_Melee_A_Slow", "Attack_A", "Attack_01"],
    "Attack_02": ["Attack_PrimaryB", "Attack_B_Med", "Attack_B_Fast", "PrimaryAttack_RA", "Melee_B", "PrimaryAttack_B_Slow", "Swing2_Medium",
                  "PrimaryAttack_B", "Primary_Melee_B_Slow", "Attack_B", "Attack_02"],
    "Attack_03": ["Attack_PrimaryC", "Attack_C_Med", "Attack_C_Fast", "PrimaryAttack_LB", "Melee_C", "PrimaryAttack_C_Slow", "Swing3_Medium",
                  "PrimaryAttack_C", "Primary_Melee_C_Slow", "Attack_C", "Attack_03", "Attack_A_Slow"],
    "Attack_Heavy": ["PrimaryAttack_FourStrikes", "Primary_Melee_E_Slow", "Melee_A_Slow", "PrimaryAttack_D_Slow", "Swing1_Slow", "Attack_D"],
    "Signature": ["R_Ability", "Ultimate_Swing_120fps", "Attack_E_SetA"],
    "HitReact": ["HitReact_Front", "Hit_React_Front"],
    "Knockdown": ["KnockUp", "Stun"],
    "Death": ["Death_A", "Death_Front"],
}


def pick_any(names, montage, candidates):
    found = pick(names, candidates + PATTERNS.get(montage, []))
    if found:
        return found
    # Last resort: any clip whose name contains the first candidate's stem (never the _MSA / Montage copies).
    stems = [c.lower() for c in candidates[:1]]
    for name, path in sorted(names.items()):
        lower = name.lower()
        if any(stem in lower for stem in stems) and not lower.endswith("_msa") and "montage" not in lower and is_base_pose(path):
            return path
    return None


def heroes():
    content = os.path.join(PROJECT, "Content")
    for entry in sorted(os.listdir(content)):
        if entry.startswith("Paragon") and os.path.isdir(os.path.join(content, entry)):
            yield entry


def body_meshes(pack):
    """The pack's skeletal meshes outside Skins (the default look first)."""
    found = []
    for path in library.list_assets("/Game/" + pack + "/Characters", recursive=True, include_folder=False):
        if "/Skins/" in path or "/Global/" in path:
            continue
        data = library.find_asset_data(path)
        if data.asset_class_path.asset_name == "SkeletalMesh":
            found.append(path.split(".")[0])
    return found


def make_montage(source_path, target_folder, name):
    sequence = unreal.load_asset(source_path)
    if not isinstance(sequence, unreal.AnimSequence):
        return None
    # Gameplay moves the characters in code: the clips stay in place.
    sequence.set_editor_property("force_root_lock", True)
    sequence.set_editor_property("enable_root_motion", False)
    library.save_loaded_asset(sequence, False)
    path = target_folder + "/AM_" + name
    if library.does_asset_exist(path):
        existing = unreal.load_asset(path)
        if existing and existing.get_editor_property("skeleton") == sequence.get_editor_property("skeleton"):
            return existing
        library.delete_asset(path)
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property("source_animation", sequence)
    factory.set_editor_property("target_skeleton", sequence.get_editor_property("skeleton"))
    montage = tools.create_asset("AM_" + name, target_folder, unreal.AnimMontage, factory)
    if montage:
        library.save_loaded_asset(montage, False)
    return montage


def setup(pack):
    meshes = body_meshes(pack)
    if not meshes:
        log("%s: no skeletal mesh (still downloading?)" % pack)
        return
    for mesh_path in meshes:
        mesh = unreal.load_asset(mesh_path)
        skeleton = mesh.get_editor_property("skeleton")
        hero_folder = "/".join(mesh_path.split("/")[:-2])  # .../Heroes/<Hero> (Meshes is the last folder)
        names = asset_names(hero_folder)
        key_name = mesh_path.split("/")[-1]
        # Minions share one animation folder per type (Melee / Ranged / Siege / Super), Dusk ones use Dawn's.
        for kind in ("Melee", "Ranged", "Siege", "Super"):
            if "_" + kind in key_name:
                kind_folder = "/Game/" + pack + "/Characters/Minions/Down_Minions/Animations/" + kind
                if library.does_directory_exist(kind_folder):
                    names = asset_names(kind_folder)
                    IDLE_KIND = ["NonCombat_Idle", "Idle", "Combat_Idle"]
                    RUN_KIND = ["Combat_JogFwd", "Combat_Jog_Fwd_Alt", "NonCombat_JogFwd", "NonCombat_Jog_Fwd", "Jog_Fwd"]
                    names["__idle"] = pick(names, IDLE_KIND)
                    names["__run"] = pick(names, RUN_KIND)
                break
        idle = names.pop("__idle", None) or pick(names, IDLE)
        run = names.pop("__run", None) or pick(names, RUN)
        if not idle:
            # Minions packs keep several characters side by side: look next to the mesh.
            names = asset_names("/".join(mesh_path.split("/")[:-1]))
            idle = pick(names, IDLE)
            run = run or pick(names, RUN)
        if not idle:
            log("%s: %s has no idle animation, skipped" % (pack, mesh_path))
            continue
        key = mesh_path.split("/")[-1]
        target = TARGET + "/" + key
        if not library.does_directory_exist(target):
            library.make_directory(target)
        made = []
        for name, candidates in MONTAGES:
            source = pick_any(names, name, candidates)
            if source and make_montage(source, target, name):
                made.append(name)
        bounds = mesh.get_bounds()
        height = bounds.box_extent.z * 2.0
        for loop in (idle, run):
            sequence = unreal.load_asset(loop) if loop else None
            if isinstance(sequence, unreal.AnimSequence):
                sequence.set_editor_property("force_root_lock", True)
                library.save_loaded_asset(sequence, False)
        profile = "CV_" + pack + ("" if len(meshes) == 1 else "_" + key)
        make_profile(profile, mesh, target, idle, run, height)
        log("%s|%s|%s|%s|%.0f|%s|%s" % (pack, profile, mesh_path, skeleton.get_path_name() if skeleton else "-", height, idle, run or "-"))
        log("%s montages: %s" % (key, ",".join(made)))


SETS = "/Game/DarkBlood/Animation/Sets/Paragon"
PROFILES = "/Game/DarkBlood/Characters/Profiles/Paragon"


def data_asset(name, folder, cls):
    path = folder + "/" + name
    if library.does_asset_exist(path):
        existing = unreal.load_asset(path)
        if isinstance(existing, cls):
            return existing
        library.delete_asset(path)
    if not library.does_directory_exist(folder):
        library.make_directory(folder)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    return tools.create_asset(name, folder, cls, factory)


def make_profile(profile, mesh, montage_folder, idle, run, height):
    """Animation set + visual profile (picked up by the asset manager: DefaultGame.ini scans both folders)."""
    set_name = profile.replace("CV_", "AS_", 1)
    animation_set = data_asset(set_name, SETS, unreal.DBAnimationSetDefinition)
    animation_set.set_editor_property("animation_set_id", set_name)
    animation_set.set_editor_property("anim_class", unreal.DBNativeLocomotionAnimInstance.static_class())
    animation_set.set_editor_property("idle_animation", unreal.load_asset(idle))
    if run:
        animation_set.set_editor_property("run_animation", unreal.load_asset(run))
    animation_set.set_editor_property("run_speed", 450.0)
    animation_set.set_editor_property("montage_folder", montage_folder)
    library.save_loaded_asset(animation_set, False)

    visual = data_asset(profile, PROFILES, unreal.DBCharacterVisualDefinition)
    visual.set_editor_property("profile_id", profile)
    visual.set_editor_property("quality_tier", unreal.DBVisualQualityTier.HERO)
    visual.set_editor_property("body_mesh", mesh)
    visual.set_editor_property("animation_set", animation_set)
    # Feet on the capsule bottom (half height 88), facing the capsule's forward; humans ~1.85 m, big creatures ~2.35 m
    # (the gameplay capsule stays the same, bosses scale the whole actor).
    target = 185.0 if height <= 230.0 else 235.0
    scale = target / max(height, 1.0)
    visual.set_editor_property("mesh_transform", unreal.Transform(unreal.Vector(0.0, 0.0, -88.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0), unreal.Vector(scale, scale, scale)))
    visual.set_editor_property("procedural_blink", False)
    library.save_loaded_asset(visual, False)


for pack in heroes():
    setup(pack)
log("done")
