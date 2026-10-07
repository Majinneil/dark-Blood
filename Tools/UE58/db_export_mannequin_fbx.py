# Run inside the Unreal Editor Python environment (UE 5.8):
#   UnrealEditor DarkBlood.uproject -ExecutePythonScript=Tools/UE58/db_export_mannequin_fbx.py -DBQuitAfter
#
# Exports the UE5 mannequin (SKM_Manny_Simple with SK_Mannequin) to SourceArt/Mannequin/SKM_Manny_Simple.fbx. Blender
# uses it as the rig template for authored bodies (Tools/UE58/blender_rig_hyper3d.py): same bone names and hierarchy,
# so the bodies import onto SK_Mannequin and play every mannequin animation of the project.
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.path.join(PROJECT, "SourceArt", "Mannequin")
os.makedirs(OUT, exist_ok=True)

mesh = unreal.load_asset("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")
task = unreal.AssetExportTask()
task.object = mesh
task.filename = os.path.join(OUT, "SKM_Manny_Simple.fbx")
task.automated = True
task.replace_identical = True
task.prompt = False
task.exporter = unreal.SkeletalMeshExporterFBX()
options = unreal.FbxExportOption()
options.collision = False
options.level_of_detail = False
task.options = options
ok = unreal.Exporter.run_asset_export_task(task)
unreal.log("DBMANNY export %s -> %s" % (ok, task.filename))
# Skeletal mesh export needs a render-capable editor (the -run=pythonscript commandlet asserts); when started with
# UnrealEditor.exe -ExecutePythonScript=... the editor closes itself afterwards.
if "-DBQuitAfter" in unreal.SystemLibrary.get_command_line():
    unreal.SystemLibrary.quit_editor()
