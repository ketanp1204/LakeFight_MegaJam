"""B01.1 original rowboats. Run this file inside Blender; +X is the bow.

Metres, flat palette, all object origins at the waterline centre (0, 0, 0).
The open cockpit is a recess in a closed, thick, manifold hull shell.
"""
import sys
import importlib
import math
import json
import hashlib

sys.path.insert(0, r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf
importlib.reload(lf)

import bpy
import bmesh
from mathutils import Vector, Matrix
import lf_b06
importlib.reload(lf_b06)


# Counterclockwise plan: square transom, broad middle, tapered raised bow.
PLAN = [(-1.58, -.38), (-1.27, -.57), (-.75, -.65),
        (-.15, -.67), (.48, -.63), (.98, -.45), (1.35, -.20),
        (1.58, 0), (1.35, .20), (.98, .45), (.48, .63),
        (-.15, .67), (-.75, .65), (-1.27, .57), (-1.58, .38)]


def sheer(x):
    # B01.1 adds 0.20 m at bow, 0.08 m at stern; continuous slope at midships.
    return .25 + .28 * max(x / 1.58, 0) ** 2 + .105 * max(-x / 1.58, 0) ** 2


def rise(x):
    return .12 * max(x / 1.58, 0) ** 3


def mesh(col, name, verts, faces, material):
    # fresh_collection removes objects, so also discard this script's orphan meshes.
    previous = bpy.data.meshes.get(name)
    if previous is not None and previous.users == 0:
        bpy.data.meshes.remove(previous)
    data = bpy.data.meshes.new(name)
    data.from_pydata(verts, [], faces)
    data.update()
    bm = bmesh.new()
    bm.from_mesh(data)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(data)
    bm.free()
    ob = bpy.data.objects.new(name, data)
    col.objects.link(ob)
    if material is not None:
        data.materials.append(lf.mat(material))
    lf.flat_shade(ob)
    return ob


def rings_mesh(col, name, rings, material, caps=False, cycle=False):
    n = len(rings[0])
    verts = [v for ring in rings for v in ring]
    faces = []
    count = len(rings) if cycle else len(rings) - 1
    for r in range(count):
        a, b = r * n, ((r + 1) % len(rings)) * n
        for i in range(n):
            j = (i + 1) % n
            faces.append((a + i, a + j, b + j, b + i))
    if caps:
        faces.extend([tuple(reversed(range(n))),
                      tuple((len(rings) - 1) * n + i for i in range(n))])
    return mesh(col, name, verts, faces, material)


def box(col, name, centre, size, material, bevel=0):
    cx, cy, cz = centre
    sx, sy, sz = (d / 2 for d in size)
    verts = [(cx + x * sx, cy + y * sy, cz + z * sz)
             for x, y, z in [(-1, -1, -1), (1, -1, -1), (1, 1, -1),
                             (-1, 1, -1), (-1, -1, 1), (1, -1, 1),
                             (1, 1, 1), (-1, 1, 1)]]
    ob = mesh(col, name, verts, [(0, 3, 2, 1), (4, 5, 6, 7),
                               (0, 1, 5, 4), (1, 2, 6, 5),
                               (2, 3, 7, 6), (3, 0, 4, 7)], material)
    if bevel:
        bpy.context.view_layer.objects.active = ob
        mod = ob.modifiers.new("Soft cut edges", "BEVEL")
        mod.width = bevel
        mod.segments = 1
        bpy.ops.object.modifier_apply(modifier=mod.name)
        lf.flat_shade(ob)
    return ob


def tube(col, name, centre, radius, thickness, material, start=0,
         end=2 * math.pi, steps=12, plane="XZ"):
    closed = abs(end - start - 2 * math.pi) < 1e-6
    rings = []
    for i in range(steps if closed else steps + 1):
        a = start + (end - start) * i / steps
        radial = Vector((math.cos(a), 0, math.sin(a))) if plane == "XZ" else Vector((0, math.cos(a), math.sin(a)))
        axis = Vector((0, 1, 0)) if plane == "XZ" else Vector((1, 0, 0))
        rings.append([tuple(Vector(centre) + radial * (radius + thickness * math.cos(j * math.tau / 6))
                            + axis * thickness * math.sin(j * math.tau / 6)) for j in range(6)])
    return rings_mesh(col, name, rings, material, caps=not closed, cycle=closed)


def build(name, paint):
    col = lf.fresh_collection(name)
    prefix = "" if paint == "PaintRed" else paint.removeprefix("Paint") + "_"
    nm = lambda part: prefix + part
    # Near-upright narrow stem, rather than pulling its keel far back.
    bottom = [(x * (.95 if x > 0 else .86), y * .61, -.25 + rise(x)) for x, y in PLAN]
    chine = [(x * (.99 if x > 0 else .97),
              math.copysign(max(0, abs(y) - (sheer(x) - (-.12 + rise(x) * .75))
                               * math.tan(math.radians(11))), y),
              -.12 + rise(x) * .75) for x, y in PLAN]
    top = [(x, y, sheer(x)) for x, y in PLAN]
    inner_top = [(x * .958, y * .91, sheer(x) - .014) for x, y in PLAN]
    inner_bottom = [(x * (.90 if x > 0 else .84), y * .57, -.15 + rise(x)) for x, y in PLAN]
    hull = rings_mesh(col, nm("Hull"), [bottom, chine, top, inner_top, inner_bottom], "Wood", caps=True)

    # A continuous dark rim, a closed rectangular-section ring.
    rail = [[(x * scale, y * scale, sheer(x) + dz) for x, y in PLAN]
            for scale, dz in [(1.012, -.023), (1.012, .02), (.945, .02), (.945, -.023)]]
    rings_mesh(col, nm("Gunwale"), rail, "WoodDark", cycle=True)

    # Twice the previous band height at each station, following the raised sheer.
    def band_ring(upper, offset):
        result = []
        for a, b, (x, y) in zip(chine, top, PLAN):
            old_span = (.25 + .08 * max(x / 1.58, 0) ** 2
                        + .025 * max(-x / 1.58, 0) ** 2) - a[2]
            z = b[2] - .045 - (0 if upper else 2 * .21 * old_span)
            t = (z - a[2]) / (b[2] - a[2])
            result.append((a[0] + (b[0] - a[0]) * t + offset * x / 1.58,
                           a[1] + (b[1] - a[1]) * t + offset * y / .67, z))
        return result
    rings_mesh(col, nm("Stripe"), [band_ring(False, .003), band_ring(True, .003),
                                   band_ring(True, .0005), band_ring(False, .0005)], paint, cycle=True)

    # A small, closed triangular breast hook / bow cap, flush with the trim.
    cap_plan = [(1.19, -.268), (1.58, 0), (1.19, .268)]
    rings_mesh(col, nm("Bow_Cap"),
               [[(x, y, sheer(x) + dz) for x, y in cap_plan] for dz in [-.018, .022]],
               paint, caps=True)

    for i in range(5):
        box(col, nm(f"Floor_Plank_{i + 1:02d}"), (-.09, (i - 2) * .145, -.111),
            (2.12 - .10 * abs(i - 2), .135, .035), "Wood", .006)
    for i, (x, width) in enumerate([(-.67, 1.10), (.52, .98)], 1):
        box(col, nm(f"Thwart_{i:02d}"), (x, 0, .145), (.29, width, .075), "WoodDark", .015)

    for side, y in [("Port", -.672), ("Starboard", .672)]:
        box(col, nm(f"Oarlock_{side}_Shoe"), (-.12, y, .265), (.15, .075, .035), "Metal", .008)
        box(col, nm(f"Oarlock_{side}_Pin"), (-.12, y, .30), (.025, .025, .06), "Metal")
        tube(col, nm(f"Oarlock_{side}_Fork"), (-.12, y, .357), .045, .012,
             "Metal", start=math.radians(140), end=math.radians(400), steps=10)
    tube(col, nm("Bow_Ring"), (1.583, 0, .405), .052, .011, "Metal", plane="YZ")

    # Join only render parts. Disconnected closed pieces retain their palette slots.
    bpy.ops.object.select_all(action="DESELECT")
    for ob in col.objects:
        ob.hide_set(False)
        ob.select_set(True)
    bpy.context.view_layer.objects.active = hull
    bpy.ops.object.join()
    hull.name = name
    hull.data.name = name + "_Mesh"

    # Original floor/stern and protected inner-plane templates for B06 walls.
    # 00 floor, 01 port, 02 starboard, 03/04 bow halves, 05 stern.
    def collision(index, centre, size, yaw=0, flare=0):
        ob = box(col, f"UCX_{name}_{index:02d}", (0, 0, 0), size, None)
        rotation = Matrix.Rotation(yaw, 4, "Z") @ Matrix.Rotation(flare, 4, "X")
        for v in ob.data.vertices:
            v.co = rotation @ v.co + Vector(centre)
        ob.data.update()
        ob["LF_CollisionRole"] = ["floor", "port wall", "starboard wall",
                                   "bow port", "bow starboard", "stern"][index]
        return ob

    collision(0, (-.09, 0, -.17), (2.12, .715, .153))
    for index, sign in [(1, -1), (2, 1)]:
        collision(index, (-.27, sign * .535, .085), (2.02, .085, .47),
                  flare=-sign * math.radians(11))
    for index, sign in [(3, -1), (4, 1)]:
        a = Vector((.70, sign * .55, .145))
        b = Vector((1.54, sign * .018, .145))
        delta = b - a
        collision(index, (a + b) / 2, (delta.length, .075, .58),
                  yaw=math.atan2(delta.y, delta.x), flare=-sign * math.radians(11))
    # Transverse stern slab, tilted to follow the transom rather than capping the cockpit.
    stern = collision(5, (-1.44, 0, .075), (.085, .82, .55))
    rot = Matrix.Rotation(math.radians(-16), 4, "Y")
    centre = Vector((-1.44, 0, .075))
    for v in stern.data.vertices:
        v.co = rot @ (v.co - centre) + centre
    stern.data.update()

    # B06: split side/bow exteriors into convex segments. The original inner
    # planes, floor 00 and stern 05 stay fixed; new names continue at 06.
    collision_segments = lf_b06.fit_collision_set(col, hull, name, sheer)

    bpy.context.view_layer.update()
    verts = [hull.matrix_world @ v.co for v in hull.data.vertices]
    lo = [min(v[i] for v in verts) for i in range(3)]
    hi = [max(v[i] for v in verts) for i in range(3)]
    bad = {}
    for ob in col.objects:
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        edges = sum(not e.is_manifold for e in bm.edges)
        if edges:
            bad[ob.name] = edges
        bm.free()
        assert not ob.modifiers, ob.name
        assert all(not p.use_smooth for p in ob.data.polygons), ob.name
        assert ob.location.length == 0, ob.name
        assert all(m.name in {"M_LF_Wood", "M_LF_WoodDark", "M_LF_" + paint, "M_LF_Metal"}
                   for m in ob.data.materials), ob.name
        if ob.name.startswith("UCX_"):
            assert len(ob.data.materials) == 0, ob.name
            assert len(ob.data.vertices) <= 24, ob.name
            if ob.name.endswith(("_00", "_05")):
                assert len(ob.data.vertices) == 8 and len(ob.data.polygons) == 6, ob.name
            # Every vertex lies on or behind every face: convex, outward-facing.
            for p in ob.data.polygons:
                assert all(p.normal.dot(v.co - p.center) < 1e-5 for v in ob.data.vertices), ob.name
    hull.data.calc_loop_triangles()
    tris = len(hull.data.loop_triangles)
    total_tris = lf.tri_count(col)
    assert not bad, bad
    assert total_tris < 2500, total_tris
    assert len(col.objects) == 13
    assert hull.name == name and hull.location.length == 0
    fingerprint = hashlib.sha256(json.dumps([
        (ob.name, [tuple(v.co) for v in ob.data.vertices],
         [tuple(p.vertices) for p in ob.data.polygons],
         [p.material_index for p in ob.data.polygons], [m.name for m in ob.data.materials])
        for ob in sorted(col.objects, key=lambda o: o.name)], sort_keys=True).encode()).hexdigest()
    report = {"asset": name, "objects": sorted(ob.name for ob in col.objects),
              "render_tris": tris, "ucx_tris": total_tris - tris, "total_tris": total_tris,
              "bounds_min_m": lo, "bounds_max_m": hi,
              "size_m": [round(b - a, 6) for a, b in zip(lo, hi)],
              "origin_above_lowest_m": round(-lo[2], 6), "nonmanifold_edges": bad,
              "geometry_sha256": fingerprint, "broadside_flare_degrees": 11,
              "gunwale_bow_rise_added_m": .20, "gunwale_stern_rise_added_m": .08,
              "collision_piece_count": 12, "collision_segments": collision_segments,
              "max_ucx_vertices": max(len(o.data.vertices) for o in col.objects if o.name.startswith("UCX_"))}
    print("[B01.1] " + json.dumps(report, sort_keys=True))
    lf.export_fbx(col, name)
    return report


if bpy.context.object and bpy.context.object.mode != "OBJECT":
    bpy.ops.object.mode_set(mode="OBJECT")
bpy.context.scene.unit_settings.system = "METRIC"
bpy.context.scene.unit_settings.scale_length = 1.0
B01_REPORT = [build("SM_LF_Rowboat", "PaintRed"), build("SM_LF_Rowboat_Blue", "PaintBlue"),
              build("SM_LF_Rowboat_Green", "PaintGreen"), build("SM_LF_Rowboat_Yellow", "PaintYellow")]
REPORTS = B01_REPORT
print("[B06] complete: four boats, unchanged render, twelve convex UCX pieces each; bow +X, waterline Z=0")
