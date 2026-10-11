# Run inside the Unreal Editor Python environment (UE 5.8) after Tools/Audio/db_prepare_audio.py:
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_import_audio.py
#
# Imports the prepared sound bank (SourceArt/Audio/<Category>/<Name>.wav) as SoundWaves under
# /Game/DarkBlood/Audio/<Category>/S_<Name>: ambience and music loop, every wave gets the engine sound class of its
# kind (SFX, Voice, Music) so the volume settings reach it. DBAudio (C++) finds the variations by name.
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "SourceArt", "Audio")
ROOT = "/Game/DarkBlood/Audio"
LOOPING = {"Ambience", "Music"}
CLASSES = {"Music": "/Engine/EngineSounds/Music", "Voice": "/Engine/EngineSounds/Voice"}

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
imported = 0
for category in sorted(os.listdir(SOURCE)):
    folder = os.path.join(SOURCE, category)
    if not os.path.isdir(folder):
        continue
    tasks = []
    for name in sorted(os.listdir(folder)):
        if not name.lower().endswith(".wav"):
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(folder, name))
        task.set_editor_property("destination_path", ROOT + "/" + category)
        task.set_editor_property("destination_name", "S_" + os.path.splitext(name)[0])
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    sound_class = unreal.load_asset(CLASSES.get(category, "/Engine/EngineSounds/SFX"))
    for task in tasks:
        path = "%s/%s/%s" % (ROOT, category, task.get_editor_property("destination_name"))
        wave = unreal.load_asset(path)
        if wave is None:
            unreal.log_warning("DBAUDIO import failed: " + path)
            continue
        wave.set_editor_property("looping", category in LOOPING)
        if sound_class:
            wave.set_editor_property("sound_class_object", sound_class)
        library.save_loaded_asset(wave, False)
        imported += 1
    unreal.log("DBAUDIO %s: %d waves" % (category, len(tasks)))
unreal.log("DBAUDIO imported %d sound waves" % imported)
