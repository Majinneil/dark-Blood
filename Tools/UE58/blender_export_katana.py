# Run with Blender 5.x in background mode:
#   blender -b "SourceArt/Fab/corrupted_dark_katana/Sword M13.blend" --python Tools/UE58/blender_export_katana.py
#
# The free Fab "Corrupted Dark Katana" (Deepanshu, Standard license, "Allows usage with AI: Yes") ships only as a
# .blend with packed 4K textures. This exports what Tools/UE58/db_import_fab_weapons.py imports:
#   SM_Katana_Corrupted.fbx  pivot at the grip center, blade along +Z, 105 cm long (fits the mannequin hand)
#   T_Katana_*.png           the packed textures at 2K
import os

import bpy
from mathutils import Matrix, Vector

OUT = os.path.join(os.path.dirname(bpy.data.filepath), "export")
os.makedirs(OUT, exist_ok=True)

sword = bpy.data.objects["sword"]
sword.data.transform(sword.matrix_world)
sword.matrix_world = Matrix.Identity(4)
heights = [v.co.z for v in sword.data.vertices]
low, high = min(heights), max(heights)
# Measured on the source model: pommel at 0.727, handle 0.912 - 1.189, tip at 2.574 (grip center 1.05).
grip = Vector((0.0, 0.0, low + (1.05 - 0.727) / (2.574 - 0.727) * (high - low)))
sword.data.transform(Matrix.Scale(1.05 / (high - low), 4) @ Matrix.Translation(-grip))

for image in bpy.data.images:
    if image.size[0] == 0:
        continue
    image.scale(2048, 2048)
    image.filepath_raw = os.path.join(OUT, image.name.replace("Material.001_", "T_Katana_").replace(".tga", "") + ".png")
    image.file_format = "PNG"
    image.save()

bpy.ops.object.select_all(action="DESELECT")
sword.select_set(True)
bpy.context.view_layer.objects.active = sword
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "SM_Katana_Corrupted.fbx"), use_selection=True, apply_unit_scale=True,
                         apply_scale_options="FBX_SCALE_UNITS", mesh_smooth_type="FACE", path_mode="STRIP", embed_textures=False)
print("DBKATANA exported to " + OUT)
