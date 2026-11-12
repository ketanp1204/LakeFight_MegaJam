"""Original B02 geometry, validation and viewport helpers. Requires lf_common.

No external geometry, modifiers, textures, or edits to unrelated scene objects.
All meshes are built explicitly below, in metres. Screenshots only hide other
objects in the viewport; exports contain the asset's own fresh collection.
"""
import bpy
import bmesh
import math
import json
import hashlib
import os
from mathutils import Vector, Matrix
import lf_common as lf

ROOT = r"D:\Projects\LakeFight\Art\Export"


class Geometry:
    def __init__(self):
        self.vertices, self.faces, self.materials = [], [], []

    def add(self, vertices, faces, material):
        offset = len(self.vertices)
        self.vertices.extend(tuple(v) for v in vertices)
        self.faces.extend(tuple(offset + i for i in f) for f in faces)
        self.materials.extend([material] * len(faces) if isinstance(material, str) else material)

    def box(self, centre, size, material="Wood", bevel=0, rotation=None):
        half = [s / 2 for s in size]
        corners = [(-1, -1, -1), (1, -1, -1), (1, 1, -1), (-1, 1, -1),
                   (-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]
        if not bevel:
            verts = [Vector([c[i] * half[i] for i in range(3)]) for c in corners]
            faces = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4),
                     (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
        else:
            assert 0 < bevel < min(half)
            verts, faces, lookup = [], [], {}
            for axis in range(3):
                for c in corners:
                    lookup[axis, c] = len(verts)
                    verts.append(Vector([c[i] * (half[i] if i == axis else half[i] - bevel)
                                         for i in range(3)]))
            # Six inset faces, twelve bevel strips, eight clipped corners: 44 tris.
            for axis in range(3):
                other = [i for i in range(3) if i != axis]
                for sign in [-1, 1]:
                    face = []
                    for a, b in [(-1, -1), (1, -1), (1, 1), (-1, 1)]:
                        c = [0, 0, 0]
                        c[axis], c[other[0]], c[other[1]] = sign, a, b
                        face.append(lookup[axis, tuple(c)])
                    faces.append(tuple(face))
            for a, b in [(0, 1), (0, 2), (1, 2)]:
                other = 3 - a - b
                for sa in [-1, 1]:
                    for sb in [-1, 1]:
                        c = [0, 0, 0]
                        c[a], c[b], c[other] = sa, sb, -1
                        d = c.copy()
                        d[other] = 1
                        faces.append((lookup[a, tuple(c)], lookup[b, tuple(c)],
                                      lookup[b, tuple(d)], lookup[a, tuple(d)]))
            faces.extend(tuple(lookup[axis, c] for axis in range(3)) for c in corners)
        rotation = rotation or Matrix.Identity(3)
        self.add([rotation @ v + Vector(centre) for v in verts], faces, material)

    def beam(self, start, end, width, depth, material="WoodDark"):
        a, b = Vector(start), Vector(end)
        rotation = (b - a).to_track_quat("Z", "Y").to_matrix()
        self.box((a + b) / 2, (width, depth, (b - a).length), material, rotation=rotation)

    def loft(self, rings, material, caps=True, cycle=False):
        n = len(rings[0])
        assert all(len(r) == n for r in rings)
        vertices = [v for ring in rings for v in ring]
        faces = []
        for r in range(len(rings) if cycle else len(rings) - 1):
            a, b = r * n, ((r + 1) % len(rings)) * n
            faces.extend((a + i, a + (i + 1) % n, b + (i + 1) % n, b + i) for i in range(n))
        if caps:
            faces.extend([tuple(reversed(range(n))),
                          tuple((len(rings) - 1) * n + i for i in range(n))])
        self.add(vertices, faces, material)

    def cylinder(self, start, end, radius, material="Wood", sides=6, end_radius=None):
        a, b = Vector(start), Vector(end)
        direction = (b - a).normalized()
        ref = Vector((0, 0, 1)) if abs(direction.z) < .9 else Vector((0, 1, 0))
        u, v = direction.cross(ref).normalized(), None
        v = direction.cross(u).normalized()
        end_radius = radius if end_radius is None else end_radius
        rings = [[tuple(c + rad * (u * math.cos(i * math.tau / sides)
                                   + v * math.sin(i * math.tau / sides))) for i in range(sides)]
                 for c, rad in [(a, radius), (b, end_radius)]]
        self.loft(rings, material)

    def prism(self, outline, thickness, material="Wood", plane="XY"):
        rings = []
        for d in [-thickness / 2, thickness / 2]:
            rings.append([(a, b, d) if plane == "XY" else (a, d, b) for a, b in outline])
        self.loft(rings, material)

    def torus(self, centre, radius, thickness, material="Metal", steps=8, tube_steps=4, plane="XZ"):
        rings = []
        for i in range(steps):
            a = i * math.tau / steps
            radial = Vector((math.cos(a), 0, math.sin(a))) if plane == "XZ" else Vector((math.cos(a), math.sin(a), 0))
            axis = Vector((0, 1, 0)) if plane == "XZ" else Vector((0, 0, 1))
            rings.append([tuple(Vector(centre) + radial * (radius + thickness * math.cos(j * math.tau / tube_steps))
                                + axis * thickness * math.sin(j * math.tau / tube_steps)) for j in range(tube_steps)])
        self.loft(rings, material, caps=False, cycle=True)

    def fit(self, size=None, base=True):
        lo = [min(v[i] for v in self.vertices) for i in range(3)]
        hi = [max(v[i] for v in self.vertices) for i in range(3)]
        scale = [size[i] / (hi[i] - lo[i]) for i in range(3)] if size else [1, 1, 1]
        self.vertices = [((v[0] - (lo[0] + hi[0]) / 2) * scale[0],
                          (v[1] - (lo[1] + hi[1]) / 2) * scale[1],
                          (v[2] - (lo[2] if base else 0)) * scale[2]) for v in self.vertices]


def finish(geometry, name, budget, origin="base centre", notes=""):
    col = lf.fresh_collection(name)
    data_name = name + "_Mesh"
    old = bpy.data.meshes.get(data_name)
    if old is not None and old.users == 0:
        bpy.data.meshes.remove(old)
    data = bpy.data.meshes.new(data_name)
    data.from_pydata(geometry.vertices, [], geometry.faces)
    palette = list(dict.fromkeys(geometry.materials))
    for material in palette:
        assert material in lf.PALETTE, material
        data.materials.append(lf.mat(material))
    for p, material in zip(data.polygons, geometry.materials):
        p.material_index = palette.index(material)
    bm = bmesh.new()
    bm.from_mesh(data)
    # Triangulation also makes nonplanar rock and fish panels explicit for FBX.
    bmesh.ops.triangulate(bm, faces=list(bm.faces))
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bad_edges = sum(not e.is_manifold for e in bm.edges)
    loose_vertices = sum(not v.link_faces for v in bm.verts)
    assert not bad_edges and not loose_vertices, (name, bad_edges, loose_vertices)
    assert all(f.calc_area() > 1e-10 for f in bm.faces), (name, "degenerate face")
    bm.to_mesh(data)
    bm.free()
    data.update()
    ob = bpy.data.objects.new(name, data)
    col.objects.link(ob)
    lf.flat_shade(ob)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    bpy.context.view_layer.update()
    tris = lf.tri_count(col)
    assert tris < budget, (name, tris, budget)
    assert len(col.objects) == 1 and ob.name == name
    assert ob.location.length == 0 and not ob.modifiers
    lo = [min(v.co[i] for v in data.vertices) for i in range(3)]
    hi = [max(v.co[i] for v in data.vertices) for i in range(3)]
    if origin == "base centre":
        assert abs(lo[2]) < 1e-6, (name, "base origin", lo)
    fingerprint = hashlib.sha256(json.dumps([
        [tuple(v.co) for v in data.vertices],
        [(tuple(p.vertices), p.material_index, p.use_smooth) for p in data.polygons],
        [m.name for m in data.materials]], sort_keys=True).encode()).hexdigest()
    path = lf.export_fbx(col, name)
    assert os.path.isfile(path) and os.path.getsize(path) > 0
    report = dict(asset=name, tris=tris, limit_exclusive=budget, objects=[ob.name],
                  bounds_min_m=[round(v, 6) for v in lo], bounds_max_m=[round(v, 6) for v in hi],
                  size_m=[round(hi[i] - lo[i], 6) for i in range(3)], origin=origin,
                  palette=[m.name for m in data.materials], nonmanifold_edges=bad_edges,
                  geometry_sha256=fingerprint, fbx=path, notes=notes)
    os.makedirs(os.path.join(ROOT, "_reports"), exist_ok=True)
    with open(os.path.join(ROOT, "_reports", name + ".json"), "w") as f:
        json.dump(report, f, indent=2)
    print("[B02] " + json.dumps(report, sort_keys=True))
    return report


def enum_set(owner, prop, desired):
    values = [i.identifier for i in owner.bl_rna.properties[prop].enum_items]
    assert desired in values, (prop, values)
    setattr(owner, prop, desired)


def capture(name, direction=(4, -6, 4)):
    """Solid material-colour viewport, about 70% frame, no non-asset geometry."""
    scene = bpy.context.scene
    for ob in scene.objects:
        ob.hide_set(ob.name != name)
    bpy.ops.object.select_all(action="DESELECT")
    ob = bpy.data.objects[name]
    area = next(a for a in bpy.context.screen.areas if a.type == "VIEW_3D")
    region = next(r for r in area.regions if r.type == "WINDOW")
    space = area.spaces.active
    enum_set(space.shading, "type", "SOLID")
    enum_set(space.shading, "color_type", "MATERIAL")
    enum_set(space.shading, "light", "STUDIO")
    enum_set(space.shading, "background_type", "VIEWPORT")
    space.shading.background_color = (.055, .065, .08)
    space.shading.show_cavity = True
    enum_set(space.shading, "cavity_type", "BOTH")
    space.overlay.show_overlays = False
    enum_set(space.region_3d, "view_perspective", "ORTHO")
    q = Vector(direction).to_track_quat("Z", "Y")
    coords = [ob.matrix_world @ v.co for v in ob.data.vertices]
    right, up = q @ Vector((1, 0, 0)), q @ Vector((0, 1, 0))
    extents = []
    for axis in [right, up]:
        projected = [v.dot(axis) for v in coords]
        extents.append(max(projected) - min(projected))
    centre = Vector([(min(v[i] for v in coords) + max(v[i] for v in coords)) / 2 for i in range(3)])
    space.region_3d.view_rotation = q
    space.region_3d.view_location = centre
    space.region_3d.view_distance = max(extents[0], extents[1] * 1.25) * 1.05
    scene.render.resolution_x = 1000
    scene.render.resolution_y = 800
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = False
    enum_set(scene.render.image_settings, "file_format", "PNG")
    directory = os.path.join(ROOT, "_shots")
    os.makedirs(directory, exist_ok=True)
    scene.render.filepath = os.path.join(directory, name + ".png")
    with bpy.context.temp_override(area=area, region=region):
        bpy.ops.render.opengl(write_still=True, view_context=True)
    print("[B02] screenshot " + scene.render.filepath)
    return scene.render.filepath
