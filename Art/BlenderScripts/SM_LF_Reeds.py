"""B02: 1.2 m clump of ten reeds, six cattail heads and ten solid leaves."""
import sys, importlib, math, random
sys.path.insert(0, r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf
importlib.reload(lf)
import lf_b02 as b
importlib.reload(b)
from mathutils import Vector

g = b.Geometry()
rng = random.Random(20261112)
heights = [1.04, .88, .96, .84, .91, .79, .75, .85, .69, .77]
for i, h in enumerate(heights):
    angle = i * 2.399963
    radius = .035 + .145 * math.sqrt(i / 9)
    start = Vector((radius * math.cos(angle), radius * math.sin(angle), 0))
    end = start + Vector((.07 * math.cos(angle), .07 * math.sin(angle), h))
    g.cylinder(start, end, .007, "Reed", sides=4, end_radius=.0045)
    if i < 6:
        rings = [[(end.x + r * math.cos(j * math.tau / 5),
                   end.y + r * math.sin(j * math.tau / 5), h + dz) for j in range(5)]
                 for dz, r in [(0, .009), (.125, .022), (.16, .009)]]
        g.loft(rings, "WoodDark")
    # Triangular solid blades have visible back faces without two-sided materials.
    z = .19 + rng.random() * .32
    root = start.lerp(end, z / h)
    direction = Vector((math.cos(angle + .7), math.sin(angle + .7), 0))
    transverse = Vector((-direction.y, direction.x, 0)) * .005
    outline = [root, root + direction * .29 + Vector((0, 0, .33)),
               root + direction * .08 + Vector((0, 0, .27))]
    g.loft([[tuple(v + transverse * sign) for v in outline] for sign in [-1, 1]], "Reed")
g.fit()
height = max(v[2] for v in g.vertices)
g.vertices = [(x, y, z * 1.2 / height) for x, y, z in g.vertices]
REPORTS = [b.finish(g, "SM_LF_Reeds", 400,
                    notes="Ten deterministic stems, six seed heads, ten solid 1 cm leaves; exactly 1.2 m tall; no alpha cards.")]
