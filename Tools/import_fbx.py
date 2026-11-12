"""Import Lake Fight FBX exports into Unreal (run inside the editor: Python console, `py` command, or Unreal MCP).

    py "<ProjectDir>/Tools/import_fbx.py"                     -> imports every Art/Export/*.fbx
    py "<ProjectDir>/Tools/import_fbx.py" SM_LF_Rowboat       -> imports one

Static meshes land in /Game/LakeFight/Meshes, materials in /Game/LakeFight/Materials/Imported.
Re-importing replaces the existing asset (same name), so Blender script -> export -> import is repeatable.
"""
import os
import sys
import unreal

project_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
export_dir = os.path.join(project_dir, "Art", "Export")
dest_mesh = "/Game/LakeFight/Meshes"

names = sys.argv[1:]
files = sorted(f for f in os.listdir(export_dir) if f.lower().endswith(".fbx")) if os.path.isdir(export_dir) else []
if names:
    files = [f for f in files if os.path.splitext(f)[0] in names]
if not files:
    unreal.log_warning(f"[lf] nothing to import from {export_dir}")

tasks = []
for f in files:
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = False
    ui.import_materials = True
    ui.import_textures = False
    ui.import_animations = False
    ui.static_mesh_import_data.combine_meshes = True
    ui.static_mesh_import_data.generate_lightmap_u_vs = True
    ui.static_mesh_import_data.auto_generate_collision = True
    ui.static_mesh_import_data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS

    t = unreal.AssetImportTask()
    t.filename = os.path.join(export_dir, f)
    t.destination_path = dest_mesh
    t.destination_name = os.path.splitext(f)[0]
    t.automated = True
    t.replace_existing = True
    t.save = True
    t.options = ui
    tasks.append(t)

if tasks:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for t in tasks:
        unreal.log(f"[lf] imported {t.destination_name}: {list(t.imported_object_paths)}")
