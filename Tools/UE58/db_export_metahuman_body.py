# Run in the Unreal Editor (UE 5.8; skeletal mesh export needs a render-capable editor, not the commandlet):
#   UnrealEditor.exe DarkBlood.uproject /Engine/Maps/Entry "-ExecCmds=py <project>/Tools/UE58/db_export_metahuman_body.py Akaza,QUIT_EDITOR"
#
# Exports the body of a built MetaHuman to SourceArt/MetaHumans/<Name>/SKM_<Name>_BodyMesh.fbx (gitignored: Epic content).
# Tools/Art/blender_make_akaza_outfit.py cuts the gown to it, so gown and body share one bind pose in the game.
import os
import sys

import unreal

NAME = sys.argv[1] if len(sys.argv) > 1 else "Akaza"
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.path.join(PROJECT, "SourceArt", "MetaHumans", NAME)
os.makedirs(OUT, exist_ok=True)

mesh = unreal.load_asset("/Game/DarkBlood/Characters/MetaHumans/Build/%s/%s/Body/SKM_%s_BodyMesh" % (NAME, NAME, NAME))
if mesh is None:
    raise SystemExit("DBBODY no built body for " + NAME)
task = unreal.AssetExportTask()
task.object = mesh
task.filename = os.path.join(OUT, "SKM_%s_BodyMesh.fbx" % NAME)
task.automated = True
task.replace_identical = True
task.prompt = False
task.exporter = unreal.SkeletalMeshExporterFBX()
options = unreal.FbxExportOption()
options.collision = False
options.level_of_detail = False
options.export_morph_targets = False
task.options = options
unreal.log("DBBODY export %s -> %s" % (unreal.Exporter.run_asset_export_task(task), task.filename))
