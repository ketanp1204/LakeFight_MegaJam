"""Shared helpers for Lake Fight Blender asset scripts (run inside Blender via execute_blender_code).

Usage at the top of an asset script:
    import sys, importlib
    sys.path.insert(0, r"<ProjectDir>/Art/BlenderScripts")
    import lf_common as lf; importlib.reload(lf)
    col = lf.fresh_collection("SM_LF_Rowboat")
    ... build meshes, link them to col, use lf.mat("Wood") ...
    lf.export_fbx(col, "SM_LF_Rowboat")
"""
import os
import bpy

# Flat low-poly palette. Names become Unreal material slot names (M_LF_<Name>).
PALETTE = {
    "Wood":      (0.45, 0.28, 0.15),
    "WoodDark":  (0.28, 0.17, 0.09),
    "PaintRed":  (0.75, 0.18, 0.14),
    "PaintBlue": (0.16, 0.36, 0.70),
    "PaintGreen": (0.20, 0.55, 0.25),
    "PaintYellow": (0.90, 0.72, 0.15),
    "Black":     (0.03, 0.03, 0.03),
    "PaintOrange": (0.95, 0.45, 0.10),
    "Moss":      (0.33, 0.42, 0.22),
    "PaintCream": (0.92, 0.86, 0.70),
    "Rope":      (0.70, 0.60, 0.40),
    "Metal":     (0.55, 0.57, 0.60),
    "Reed":      (0.42, 0.55, 0.22),
    "Rock":      (0.45, 0.45, 0.42),
    "Skin":      (0.86, 0.66, 0.52),
    "Cloth":     (0.30, 0.42, 0.30),
}

# B02.1: reserved for boats/stripes and team flags. The director explicitly
# permits PaintYellow on SM_LF_Fish_Gold as the bonus-fish exception.
TEAM_COLOURS = frozenset({"PaintRed", "PaintBlue", "PaintGreen", "PaintYellow"})

EXPORT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Export")


def fresh_collection(name):
    """Delete the collection (and its objects) if it exists, then create it empty. Makes scripts idempotent."""
    old = bpy.data.collections.get(name)
    if old:
        for ob in list(old.objects):
            bpy.data.objects.remove(ob, do_unlink=True)
        bpy.data.collections.remove(old)
    col = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(col)
    return col


def mat(name):
    """Get or create a flat material M_LF_<name> from the palette."""
    full = f"M_LF_{name}"
    m = bpy.data.materials.get(full)
    if m is None:
        m = bpy.data.materials.new(full)
        m.use_nodes = True
        bsdf = next((n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
        r, g, b = PALETTE[name]
        if bsdf:
            bsdf.inputs["Base Color"].default_value = (r, g, b, 1.0)
            bsdf.inputs["Roughness"].default_value = 0.85
        m.diffuse_color = (r, g, b, 1.0)
    return m


def link(ob, col):
    for c in list(ob.users_collection):
        c.objects.unlink(ob)
    col.objects.link(ob)
    return ob


def flat_shade(ob):
    for p in ob.data.polygons:
        p.use_smooth = False


def tri_count(col):
    dg = bpy.context.evaluated_depsgraph_get()
    total = 0
    for ob in col.objects:
        if ob.type == "MESH":
            me = ob.evaluated_get(dg).to_mesh()
            me.calc_loop_triangles()
            total += len(me.loop_triangles)
            ob.evaluated_get(dg).to_mesh_clear()
    return total


def export_fbx(col, asset_name):
    """Export only this collection's objects to Art/Export/<asset_name>.fbx (1 unit = 1 m, Z up)."""
    os.makedirs(EXPORT_DIR, exist_ok=True)
    path = os.path.normpath(os.path.join(EXPORT_DIR, f"{asset_name}.fbx"))
    bpy.ops.object.select_all(action="DESELECT")
    for ob in col.objects:
        ob.select_set(True)
    bpy.ops.export_scene.fbx(
        filepath=path,
        use_selection=True,
        apply_scale_options="FBX_SCALE_ALL",
        axis_forward="-X",
        axis_up="Z",
        mesh_smooth_type="FACE",
        use_mesh_modifiers=True,
        add_leaf_bones=False,
        bake_anim=False,
    )
    print(f"[lf] exported {path} tris={tri_count(col)}")
    return path
