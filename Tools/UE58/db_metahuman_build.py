# Unattended MetaHuman build on this PC (UE 5.8; see docs/METAHUMANS.md for the full command line):
#   UnrealEditor.exe DarkBlood.uproject /Engine/Maps/Entry
#     -dpcvars=Editor.AsynctextureCompilationMaxConcurrency=1,r.SkinCache.Mode=0,D3D12.ResidencyManagement=0,D3D12.ResourcesStartResident=1
#     "-ExecCmds=py <project>/Tools/UE58/db_metahuman_build.py finish Akaza rebuild" -nosplash -unattended -nosound
#
# Runs another Tools/UE58 script (default db_metahuman_cast.py, arguments as given) only after the editor has completed
# some frames, then quits the editor. Started straight from -ExecCmds the build stays in frame 0 from start to end.
import os
import runpy
import sys

import unreal

SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "db_metahuman_cast.py")
WAIT_TICKS = 120
arguments = sys.argv[1:]
ticks = 0
handle = None


def on_tick(delta):
    global ticks
    ticks += 1
    if ticks < WAIT_TICKS:
        return
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.log("DBBUILD starting after %d ticks: %s" % (ticks, " ".join(arguments)))
    try:
        sys.argv = [SCRIPT] + arguments
        runpy.run_path(SCRIPT, run_name="__main__")
        unreal.log("DBBUILD done")
    except BaseException as error:
        unreal.log_error("DBBUILD failed: %r" % (error,))
    unreal.SystemLibrary.execute_console_command(None, "QUIT_EDITOR")


handle = unreal.register_slate_post_tick_callback(on_tick)
unreal.log("DBBUILD armed: %s" % " ".join(arguments))
