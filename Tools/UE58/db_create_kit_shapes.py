# Run inside the Unreal Editor Python environment (UE 5.8):
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_create_kit_shapes.py
#
# Nanite copies of the engine basic shapes for the procedural art kit (FDBArtBatcher). The engine shapes have no Nanite
# data, so thousands of kit pieces (walls, beams, roofs, paths) were drawn the classic way - the render thread cost that
# kept settlements far below the frame rate target. The copies live in /Game/DarkBlood/Art/Kit/Shapes.
import unreal

FOLDER = "/Game/DarkBlood/Art/Kit/Shapes"
SHAPES = {"SM_DB_Cube": "/Engine/BasicShapes/Cube", "SM_DB_Cylinder": "/Engine/BasicShapes/Cylinder", "SM_DB_Sphere": "/Engine/BasicShapes/Sphere",
          "SM_DB_Cone": "/Engine/BasicShapes/Cone", "SM_DB_Plane": "/Engine/BasicShapes/Plane"}

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
for name, source in SHAPES.items():
    mesh = unreal.load_asset(FOLDER + "/" + name)
    if mesh is None:
        mesh = tools.duplicate_asset(name, FOLDER, unreal.load_asset(source))
    settings = mesh.get_editor_property("nanite_settings")
    settings.set_editor_property("enabled", True)
    mesh.set_editor_property("nanite_settings", settings)
    library.save_loaded_asset(mesh, False)
    unreal.log("DBSHAPES %s nanite=%s" % (name, mesh.get_editor_property("nanite_settings").get_editor_property("enabled")))
