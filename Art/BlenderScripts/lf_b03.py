"""B03 original mesh helpers. Metres, palette only, no external assets.

Shore convention: straight +X tangent, land +Y, water -Y; Z=0 waterline.
Corner shoreline radius 4m, quarter-circle +X to +Y, land inward for Out,
outward for In. At each end the identical six-metre profile accepts a
rotated/translated straight. Island diameter includes the underwater toe.
"""
import math
import json
import os
import random
import bpy
from mathutils import Vector, Matrix
import lf_common as lf
import lf_b02 as b


def finish(g, name, budget=1000, origin="base centre", notes="", **evidence):
    report = b.finish(g, name, budget, origin, notes)
    report.update(evidence)
    with open(os.path.join(b.ROOT, "_reports", name + ".json"), "w") as f:
        json.dump(report, f, indent=2)
    return report


def shell(g, outline, material="Reed", thickness=.01):
    """Closed leaf with true thickness perpendicular to the final blade plane."""
    outline = [Vector(v) for v in outline]
    n = (outline[1]-outline[0]).cross(outline[2]-outline[0]).normalized()
    assert n.length > .99
    start = len(g.vertices)
    g.loft([[tuple(v + n * sign * thickness/2) for v in outline] for sign in [-1, 1]], material)
    actual = [(Vector(g.vertices[start+len(outline)+i])-Vector(g.vertices[start+i])).dot(n)
              for i in range(len(outline))]
    assert max(abs(t-thickness) for t in actual) < 2e-6
    return min(actual), max(actual)


PROFILE = [(-3., -1.), (-1.4, -.42), (0., 0.), (1.15, .42), (3., .60)]


def shore(kind):
    g = b.Geometry()
    n = 9 if kind != "Straight" else 5
    rings = []
    for i in range(n):
        t = i / (n-1)
        if kind == "Straight":
            ring = [(8*t-4, y, z) for y, z in PROFILE]
            ring += [(8*t-4, 3., -1.25), (8*t-4, -3., -1.25)]
        else:
            angle = t * math.pi/2
            inward = kind == "CornerOut"
            cross = PROFILE + [(3., -1.25), (-3., -1.25)]
            ring = [((4-y if inward else 4+y)*math.cos(angle),
                     (4-y if inward else 4+y)*math.sin(angle), z) for y,z in cross]
        rings.append(ring)
    verts = [v for ring in rings for v in ring]
    faces, mats = [], []
    width = len(rings[0])
    for i in range(n-1):
        for j in range(width):
            faces.append((i*width+j, i*width+(j+1)%width,
                          (i+1)*width+(j+1)%width, (i+1)*width+j))
            mats.append("Rope" if j < 3 else "Moss" if j == 3 else "WoodDark")
    faces += [tuple(reversed(range(width))), tuple((n-1)*width+j for j in range(width))]
    mats += ["WoodDark"]*2
    g.add(verts, faces, mats)
    connectors = ([[-4,0,0],[4,0,0]] if kind == "Straight" else [[4,0,0],[0,4,0]])
    return finish(g, "SM_LF_Shore_"+kind, origin="Z=0 waterline; straight midpoint / corner arc centre",
                  notes="8m straight or R4m quarter-turn. Shared profile: underwater toe Z=-1, waterline Z=0, grass Z=0.42..0.60. Buried closing sole Z=-1.25; end caps are hidden by adjoining modules.",
                  profile_yz_m=PROFILE, shoreline_connectors_m=connectors,
                  corner_land_side="inside radius" if kind=="CornerOut" else "outside radius" if kind=="CornerIn" else "+Y")


def island(name, diameter, seed):
    rng = random.Random(seed)
    n = 16 if diameter < 10 else 20
    radius = diameter/2
    variations = [rng.uniform(.88,1.) for _ in range(n)]
    # Exact diameter on X, softer asymmetric outline everywhere else.
    variations[0] = variations[n//2] = 1.
    heights = [-1., -.32, .08, .36 if diameter<10 else .55]
    scales = [1., .90, .76, .60]
    verts = []
    for k, (z, scale) in enumerate(zip(heights, scales)):
        for j in range(n):
            a = j*math.tau/n
            verts.append((radius*scale*variations[j]*math.cos(a),
                          radius*scale*variations[j]*math.sin(a),
                          z + (rng.uniform(-.04,.04) if k==3 else 0)))
    verts.append((.14*radius, -.08*radius, .52 if diameter<10 else .78))
    faces, mats = [], []
    for k in range(3):
        for j in range(n):
            faces.append((k*n+j, k*n+(j+1)%n, (k+1)*n+(j+1)%n, (k+1)*n+j))
            mats.append("Rope" if k<2 else "Moss")
    for j in range(n):
        faces.append((3*n+j, 3*n+(j+1)%n, 4*n))
        mats.append("Moss" if j%4 else "Reed")
    faces.append(tuple(reversed(range(n))))
    mats.append("WoodDark")
    g = b.Geometry()
    g.add(verts,faces,mats)
    return finish(g, name, origin="waterline centre Z=0",
                  notes="Irregular low grassy crown, continuous sandy rim and sloping closed underwater toe. Diameter includes toe, not just dry land.",
                  nominal_diameter_m=diameter, shoreline_z_m=0, underwater_toe_z_m=-1.)


def crown(g, centre, radius, height, seed, sides=7):
    rng = random.Random(seed)
    c=Vector(centre)
    shape=[(-.48,.30),(-.26,.88),(.17,1.),(.47,.47)]
    radii=[rng.uniform(.85,1.10) for _ in range(sides)]
    rings=[[(c.x+radius*s*radii[j]*math.cos(j*math.tau/sides),
             c.y+radius*s*radii[j]*math.sin(j*math.tau/sides),
             c.z+height*z+rng.uniform(-.06,.06)) for j in range(sides)] for z,s in shape]
    g.loft(rings,"Moss")


def view_quaternion(direction):
    eye=Vector(direction).normalized()
    up=Vector((0,1,0)) if abs(eye.z)>.99 else Vector((0,0,1))
    right=up.cross(eye).normalized()
    up=eye.cross(right).normalized()
    return Matrix((right,up,eye)).transposed().to_quaternion()


def shot(name, direction=(4,-6,4), suffix=""):
    """Use B02 capture with an explicit world-up frame (correct top-view labels)."""
    path=b.capture(name,direction)
    area=next(a for a in bpy.context.screen.areas if a.type=="VIEW_3D")
    region=next(r for r in area.regions if r.type=="WINDOW")
    area.spaces.active.region_3d.view_rotation=view_quaternion(direction)
    bpy.context.scene.render.filepath=os.path.join(b.ROOT,"_shots",name+suffix+".png")
    with bpy.context.temp_override(area=area,region=region):
        bpy.ops.render.opengl(write_still=True, view_context=True)
    return bpy.context.scene.render.filepath
