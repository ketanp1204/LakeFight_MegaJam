"""B02.1: round orange/cream marker buoy with a solid swallowtail flag."""
import sys, importlib, math
sys.path.insert(0, r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf
importlib.reload(lf)
import lf_b02 as b
importlib.reload(b)

g = b.Geometry()
profile = [(0, .05), (.08, .205), (.22, .29), (.34, .29), (.48, .205), (.56, .055)]
rings = [[(r * math.cos(i * math.tau / 10), r * math.sin(i * math.tau / 10), z)
          for i in range(10)] for z, r in profile]
colours = [colour for colour in ["PaintOrange", "PaintOrange", "PaintCream", "PaintOrange", "PaintOrange"]
           for _ in range(10)] + ["PaintOrange", "PaintOrange"]
g.loft(rings, colours)
g.cylinder((0, 0, .53), (0, 0, 1.12), .014, "PaintCream", sides=6)
g.prism([(.015, 1.12), (.34, 1.07), (.28, 1.00), (.015, 1.02)], .012, "PaintOrange", plane="XZ")
g.prism([(.015, 1.02), (.28, 1.00), (.34, .92), (.015, .94)], .012, "PaintCream", plane="XZ")
g.box((0, -.232, .145), (.06, .035, .045), "Metal")
g.torus((0, -.26, .145), .045, .009, "Metal")
REPORTS = [b.finish(g, "SM_LF_Buoy", 300,
                    notes="Round 0.58 m body, cream equatorial band, orange/cream swallowtail flag, metal mooring eye. Base origin, overall height 1.12 m.")]
