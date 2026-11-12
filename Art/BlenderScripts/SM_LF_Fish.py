"""B02.1: 0.4 m fish, black domed eyes and three palette-only colourways."""
import sys, importlib, math
sys.path.insert(0, r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf
importlib.reload(lf)
import lf_b02 as b
importlib.reload(b)

REPORTS = []
for variant, body, fins in [("A", "Reed", "WoodDark"), ("B", "Metal", "PaintOrange"),
                            ("Gold", "PaintYellow", "PaintOrange")]:
    g = b.Geometry()
    stations = [(-.115, .019, .033), (-.055, .043, .062),
                (.055, .045, .070), (.145, .026, .044), (.20, .009, .016)]
    rings = [[(x, w * math.cos(i * math.tau / 6), .103 + h * math.sin(i * math.tau / 6))
              for i in range(6)] for x, w, h in stations]
    colours = ["PaintCream" if i >= 3 else body for _ in range(4) for i in range(6)]
    g.loft(rings, colours + [body, body])
    tail = [(-.20, .033), (-.105, .076), (-.105, .13), (-.20, .173), (-.17, .103)]
    g.prism(tail, .014, fins, plane="XZ")
    g.prism([(-.045, .151), (.018, .219), (.075, .158)], .012, fins, plane="XZ")
    g.prism([(-.025, .05), (.035, 0), (.085, .055)], .012, fins, plane="XZ")
    for sign in [-1, 1]:
        # Eight-sided spherical dome, not stacked cylinders. The 1.7 mm cream
        # annulus shares the globe topology; no metallic bezel or central hole.
        rings = [[(.14 + r * math.cos(i * math.tau / 8), sign * (.023 + depth),
                   .124 + r * math.sin(i * math.tau / 8)) for i in range(8)]
                 for r, depth in [(.016, 0), (.0143, .002), (.010, .012)]]
        vertices = [v for ring in rings for v in ring] + [(.14, sign * .040, .124)]
        faces, eye_colours = [], []
        for level, colour in [(0, "PaintCream"), (1, "Black")]:
            for i in range(8):
                j = (i + 1) % 8
                faces.append((level * 8 + i, level * 8 + j, (level + 1) * 8 + j, (level + 1) * 8 + i))
                eye_colours.append(colour)
        faces.append(tuple(reversed(range(8))))
        eye_colours.append("PaintCream")
        for i in range(8):
            faces.append((16 + i, 16 + (i + 1) % 8, 24))
            eye_colours.append("Black")
        g.add(vertices, faces, eye_colours)
    REPORTS.append(b.finish(g, "SM_LF_Fish_" + variant, 200,
                            notes="0.4 m nose-to-tail; eight-sided black domed eyes with thin cream rings; shared geometry. Gold is the director-approved PaintYellow bonus-fish exception."))
