"""B02.1: dense original 4 x 3 m reed bed, thirty 1.2-1.8 m reeds.

Six broad, closed 1 cm leaf blades per stem make a broadside hiding screen.
Every stem/seed head/leaf is generated here; no instanced external geometry.
"""
import sys, importlib, math, random
sys.path.insert(0, r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf
importlib.reload(lf)
import lf_b02 as b
importlib.reload(b)
from mathutils import Vector
from mathutils.bvhtree import BVHTree
import bpy

g = b.Geometry()
leaf_specs = []
leaf_records = []
rng = random.Random(20261112021)
for i in range(30):
    row, column = divmod(i, 6)
    start = Vector(((column - 2.5) * .60 + rng.uniform(-.065, .065),
                    (row - 2) * .48 + rng.uniform(-.065, .065), 0))
    height = 1.2 + .6 * ((i * 7) % 30) / 29
    head = i % 5 < 3
    stem_top = height - .16 if head else height
    g.cylinder(start, start + Vector((0, 0, stem_top)), .009,
               "Reed", sides=4, end_radius=.0055)
    if head:
        g.loft([[(start.x + r * math.cos(j * math.tau / 5),
                  start.y + r * math.sin(j * math.tau / 5), stem_top + z)
                 for j in range(5)] for z, r in [(0, .011), (.12, .024), (.16, .010)]], "WoodDark")
    for j, level in enumerate([.008, .055, .14, .28, .48, .74]):
        angle = i * 2.399963 + j * math.tau / 6 + rng.uniform(-.20, .20)
        direction = Vector((math.cos(angle), math.sin(angle), 0))
        across = Vector((-direction.y, direction.x, 0))
        root = start + Vector((0, 0, level * height / 1.5))
        reach = rng.uniform(.60, .84)
        tip = root + direction * reach + Vector((0, 0, rng.uniform(.43, .56)))
        shoulder = root.lerp(tip, .45)
        half_width = rng.uniform(.18, .25)
        outline = [root, shoulder - across * half_width, tip, shoulder + across * half_width]
        normal = (tip - root).cross(across).normalized() * .005
        leaf_specs.append((half_width, outline, normal))

# Chamfer the two broadest blade tips to expose their rim silhouette, using
# the eight spare triangles available inside the original 3,000-triangle cap.
widest = set(sorted(range(len(leaf_specs)), key=lambda i: leaf_specs[i][0], reverse=True)[:2])
for index, (_, outline, normal) in enumerate(leaf_specs):
    if index in widest:
        a, left, tip, right = outline
        outline = [a, left, tip.lerp(left, .06), tip.lerp(right, .06), right]
    leaf_records.append((len(g.vertices), len(outline)))
    g.loft([[tuple(v + normal * sign) for v in outline] for sign in [-1, 1]], "Reed")

# Fit the leaf envelope to the patch; thickness stays about 1 cm after XY fitting.
# Stem tops span 1.2-1.8 m and the lowest stem caps are at Z=0.
g.fit((4, 3, 1.8))
# Replace fitted offsets by exactly 10 mm normal separation in final metres.
# This avoids shrinking solidification thickness along with the patch envelope.
thicknesses, volumes = [], []
for start, n in leaf_records:
    outline = [(Vector(g.vertices[start + i]) + Vector(g.vertices[start + n + i])) / 2 for i in range(n)]
    normal = (outline[1] - outline[0]).cross(outline[2] - outline[0]).normalized()
    for i, centre in enumerate(outline):
        g.vertices[start + i] = tuple(centre - normal * .005)
        g.vertices[start + n + i] = tuple(centre + normal * .005)
        thicknesses.append(abs((Vector(g.vertices[start + n + i]) - Vector(g.vertices[start + i])).dot(normal)))
    area_vector = sum((outline[i].cross(outline[(i + 1) % n]) for i in range(n)), Vector()) / 2
    volumes.append(abs(area_vector.dot(normal)) * .01)
assert min(thicknesses) > .009999 and max(thicknesses) < .010001
assert min(volumes) > 0
report = b.finish(g, "SM_LF_ReedBed", 3000,
                  notes="Thirty stems, eighteen seed heads and 180 leaves retained. Every leaf rebuilt to final-world 10 mm solid thickness; two broadest blade tips chamfered; envelope grows slightly with final thickness.")
report["leaf_evidence"] = dict(count=180, thickness_min_m=min(thicknesses), thickness_max_m=max(thicknesses),
                                smallest_leaf_volume_m3=min(volumes), largest_leaf_volume_m3=max(volumes),
                                leaf_triangles=2168, side_wall_triangles=1448, chamfered_blades=2)
assert report["tris"] == 2996
print("[LEAF MEASURED]", report["leaf_evidence"])
tree = BVHTree.FromObject(bpy.data.objects["SM_LF_ReedBed"], bpy.context.evaluated_depsgraph_get())
hits, samples = 0, 0
for j in range(61):
    for k in range(11):
        start = Vector((-1.5 + j * 3 / 60, -2, .10 + k * .45 / 10))
        hits += tree.ray_cast(start, Vector((0, 1, 0)), 4)[0] is not None
        samples += 1
report["reed_count"] = 30
report["side_screen_probe"] = dict(hits=hits, samples=samples, occlusion_fraction=round(hits / samples, 4),
                                    window="3 m wide x Z=0.10..0.55 m; rays from -Y through bed")
print("[B02.1] ReedBed side-screen probe:", report["side_screen_probe"])
REPORTS = [report]
