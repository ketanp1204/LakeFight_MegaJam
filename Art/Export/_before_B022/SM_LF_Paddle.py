"""B02: original chunky 1.6 m paddle; origin at grip, blade along +X."""
import sys, importlib, math
import bpy
from mathutils import Vector
sys.path.insert(0, r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf
importlib.reload(lf)
import lf_b02 as b
importlib.reload(b)

g = b.Geometry()
g.box((.035, 0, 0), (.07, .18, .065), "WoodDark", bevel=.009)
g.cylinder((.055, 0, 0), (1.145, 0, 0), .020, "Wood", sides=8)
blade = [(1.055, -.034), (1.19, -.115), (1.47, -.145), (1.60, -.10),
         (1.60, .10), (1.47, .145), (1.19, .115), (1.055, .034)]
blade = [(x, y * 1.3) for x, y in blade]
# Five perimeter rings give a genuinely rounded/chamfered solid blade edge.
# The outermost mid-edge keeps the 1.6 m length and 0.377 m blade beam.
blade_start = len(g.vertices)
rings = []
for inset, z_fraction in [(.96, -.5), (.99, -.35), (1, 0), (.99, .35), (.96, .5)]:
    ring = []
    for x, y in blade:
        px, py = 1.3275 + (x - 1.3275) * inset, y * inset
        thickness = .027 - .004 * (px - 1.055) / .545
        ring.append((px, py, thickness * z_fraction))
    rings.append(ring)
g.loft(rings, "Wood")
for x in [.93, .97]:
    g.cylinder((x, 0, 0), (x + .018, 0, 0), .024, "WoodDark", sides=8)
REPORTS = [b.finish(g, "SM_LF_Paddle", 300, origin="grip; blade +X",
                    notes="1.6 m grip-to-tip; constant 4 cm octagonal shaft; blade 0.377 m wide (1.3x B02), 2.7 to 2.3 cm tapered, with four rounded edge facets.")]
REPORTS[0]["part_measurements_m"] = {
    "shaft_diameter": .04, "blade_width": .377,
    "blade_root_thickness": abs(rings[-1][0][2] - rings[0][0][2]),
    "blade_tip_thickness": abs(rings[-1][3][2] - rings[0][3][2])}
assert REPORTS[0]["tris"] == 204
print("[PADDLE MEASURED]", REPORTS[0]["part_measurements_m"])


def build_b02_reference():
    """Screenshot-only reconstruction of the original B02 script's exact values.

    Not exported. The historical B02 manifest records its 156 tris and bounds.
    """
    old = b.Geometry()
    old.box((.035, 0, 0), (.07, .18, .065), "WoodDark", bevel=.009)
    old.cylinder((.055, 0, 0), (1.145, 0, 0), .027, "Wood", sides=8, end_radius=.034)
    old.prism([(1.055, -.034), (1.19, -.115), (1.47, -.145), (1.60, -.10),
               (1.60, .10), (1.47, .145), (1.19, .115), (1.055, .034)], .042, "Wood")
    for x in [.93, .97]:
        old.cylinder((x, 0, 0), (x + .018, 0, 0), .031, "WoodDark", sides=8)
    col = lf.fresh_collection("_B022_PaddleReference")
    ob = b.make_object(old, "B02_Paddle_Reference", col)
    assert lf.tri_count(col) == 156
    return ob, col


def capture_b02_comparison():
    """Labelled broad-face and edge-profile views at the same scale. No export."""
    reference, col = build_b02_reference()
    paddle = bpy.data.objects["SM_LF_Paddle"]
    original = paddle.matrix_world.copy()
    scene = bpy.context.scene
    area = next(a for a in bpy.context.screen.areas if a.type == "VIEW_3D")
    region = next(r for r in area.regions if r.type == "WINDOW")
    labels = []
    for i in range(2):
        data = bpy.data.curves.new("B022_ComparisonLabel_" + str(i), "FONT")
        data.size = .035
        ob = bpy.data.objects.new(data.name, data)
        col.objects.link(ob)
        data.materials.append(lf.mat("PaintCream"))
        labels.append(ob)
    try:
        for ob in scene.objects:
            ob.hide_set(ob.name not in {o.name for o in col.objects} | {paddle.name})
        bpy.ops.object.select_all(action="DESELECT")
        b.enum_set(area.spaces.active.region_3d, "view_perspective", "ORTHO")
        area.spaces.active.region_3d.view_location = (.8, 0, 0)
        area.spaces.active.region_3d.view_distance = 1.85
        scene.render.resolution_x, scene.render.resolution_y = 1400, 850
        scene.render.resolution_percentage = 100
        for side in [False, True]:
            if side:
                reference.location, paddle.location = (0, 0, .17), (0, 0, -.17)
                labels[0].data.body = "B02: shaft 54-68 mm / blade 42 mm / 156 tris"
                labels[1].data.body = "Revised: shaft 40 mm / blade 27-23 mm / 204 tris"
                for ob, z in zip(labels, [.245, -.10]):
                    ob.location, ob.rotation_euler = (0, -.25, z), (math.pi / 2, 0, 0)
                direction, suffix = (0, -1, 0), "Side"
            else:
                reference.location, paddle.location = (0, .27, 0), (0, -.27, 0)
                labels[0].data.body = "B02: blade 0.290 m wide / 156 tris"
                labels[1].data.body = "Revised: blade 0.377 m wide / 204 tris"
                for ob, y in zip(labels, [.465, -.51]):
                    ob.location, ob.rotation_euler = (0, y, .05), (0, 0, 0)
                direction, suffix = (0, 0, 1), "Face"
            area.spaces.active.region_3d.view_rotation = Vector(direction).to_track_quat("Z", "Y")
            scene.render.filepath = b.ROOT + r"\_shots\SM_LF_Paddle_B02_Comparison_" + suffix + ".png"
            bpy.context.view_layer.update()
            with bpy.context.temp_override(area=area, region=region):
                bpy.ops.render.opengl(write_still=True, view_context=True)
            print("[COMPARISON]", scene.render.filepath)
    finally:
        paddle.matrix_world = original
        for ob in list(col.objects):
            bpy.data.objects.remove(ob, do_unlink=True)
        bpy.data.collections.remove(col)
        bpy.context.view_layer.update()
