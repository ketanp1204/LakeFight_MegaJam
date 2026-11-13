"""B04 original socket props and a synthetic Manny fitting proxy at 0.75 / 135cm.

Coordinate convention: +X character forward, +Y character right, +Z up.
Attachment origins are assumed bone pivots in this aligned socket frame.
This is not an extracted Manny rig. Raw Manny bone axes remain unverified.
"""
import bpy, math, os, json, hashlib
from mathutils import Vector, Matrix
import lf_common as lf
import lf_b02 as b

ROOT=b.ROOT
TEAMS=["Red","Blue","Green","Yellow"]
TARGET_SCALE=.75
SOURCE_PIVOTS={"head":(0.,0.,1.59),"spine_03":(0.,0.,1.35),"hand_r":(.05,.47,.94)}
PIVOTS={bone:tuple(v*TARGET_SCALE for v in p) for bone,p in SOURCE_PIVOTS.items()}


def tube_path(g,points,radii,material,sides=6):
    points=[Vector(p) for p in points]
    rings=[]
    for i,(p,r) in enumerate(zip(points,radii)):
        tangent=(points[min(i+1,len(points)-1)]-points[max(0,i-1)]).normalized()
        ref=Vector((1,0,0)) if abs(tangent.x)<.9 else Vector((0,1,0))
        u=tangent.cross(ref).normalized()
        v=tangent.cross(u).normalized()
        rings.append([tuple(p+r*(u*math.cos(j*math.tau/sides)+v*math.sin(j*math.tau/sides))) for j in range(sides)])
    g.loft(rings,material)


def ellipse_ring(rx,ry,z,n=12,wave=0):
    return [(rx*math.cos(i*math.tau/n),ry*math.sin(i*math.tau/n),
             z+wave*(math.sin(i*math.tau/n*3)+.45*math.cos(i*math.tau/n*2))) for i in range(n)]


def belt(g,rx,ry,z,height,material,n=12,cx=0):
    rings=[]
    for dx,dz in [(0,0),(.006,0),(.006,height),(0,height)]:
        rings.append([(cx+(rx+dx)*math.cos(j*math.tau/n),(ry+dx)*math.sin(j*math.tau/n),z+dz) for j in range(n)])
    g.loft(rings,material,caps=False,cycle=True)


def belt_rect(g,rx,ry,z,height,material,cx=0):
    """Chamfered rectangular strap, clear of the box-shaped flotation pillows."""
    rings=[]
    for d,dz in [(0,0),(.006,0),(.006,height),(0,height)]:
        x,y=rx+d,ry+d
        bevel=.04
        outline=[(-x+bevel,-y),(x-bevel,-y),(x,-y+bevel),(x,y-bevel),
                 (x-bevel,y),(-x+bevel,y),(-x,y-bevel),(-x,-y+bevel)]
        rings.append([(cx+a,b,z+dz) for a,b in outline])
    g.loft(rings,material,caps=False,cycle=True)


def finish(g,name,bone,notes,offset_description,team=None):
    # Authored dimensions already fit 135cm Manny. Avoid applying .75 again in UE.
    g.vertices=[tuple(v*TARGET_SCALE for v in xyz) for xyz in g.vertices]
    col=lf.fresh_collection(name)
    ob=b.make_object(g,name,col)
    bpy.context.scene.unit_settings.system="METRIC"
    bpy.context.scene.unit_settings.scale_length=1.
    bpy.context.view_layer.update()
    tris=lf.tri_count(col)
    assert 0<tris<600,(name,tris)
    assert not ob.modifiers and ob.location.length==0
    palette=sorted(m.name for m in ob.data.materials)
    used_team={"M_LF_"+c for c in lf.TEAM_COLOURS}&set(palette)
    assert used_team==({"M_LF_Paint"+team} if team else set()),(name,used_team)
    coords=[v.co for v in ob.data.vertices]
    lo=[min(v[i] for v in coords) for i in range(3)]
    hi=[max(v[i] for v in coords) for i in range(3)]
    rgb={m.name:list(next(n for n in m.node_tree.nodes if n.type=="BSDF_PRINCIPLED").inputs["Base Color"].default_value)[:3] for m in ob.data.materials}
    for m in ob.data.materials:
        key=m.name.removeprefix("M_LF_")
        assert max(abs(rgb[m.name][i]-lf.PALETTE[key][i]) for i in range(3))<1e-5
    attachment=dict(bone=bone,proxy_pivot_m=PIVOTS[bone],
                    axes="aligned socket: +X forward, +Y right, +Z up; raw Manny bone axes not verified",
                    preview_relative_location_cm=[0,0,0],preview_relative_rotation_deg=[0,0,0],
                    baked_mesh_offset=offset_description,
                    target="UE5 Manny component scaled 0.75; nominal 135cm tall",
                    authored_size_scale=TARGET_SCALE,
                    ue_scale_rule="Already authored at target size. Use absolute world scale 1 on prop, or relative scale 1.333333 under a 0.75-scaled mesh, to avoid double shrinking.")
    ob["LF_AttachBone"]=bone
    ob["LF_SocketAssumption"]=json.dumps(attachment)
    fingerprint=hashlib.sha256(json.dumps(dict(vertices=[tuple(v.co) for v in ob.data.vertices],
        faces=[(list(p.vertices),p.material_index) for p in ob.data.polygons],palette=palette,rgb=rgb),sort_keys=True).encode()).hexdigest()
    path=lf.export_fbx(col,name)
    report=dict(asset=name,tris=tris,limit_exclusive=600,bounds_min_m=lo,bounds_max_m=hi,
                size_m=[hi[i]-lo[i] for i in range(3)],origin="assumed "+bone+" pivot; baked offset",
                attachment=attachment,palette=palette,material_linear=rgb,team=team,
                nonmanifold_edges=0,degenerate_faces=0,flat_shaded=True,
                geometry_sha256=fingerprint,fbx=path,notes=notes)
    os.makedirs(os.path.join(ROOT,"_reports"),exist_ok=True)
    with open(os.path.join(ROOT,"_reports",name+".json"),"w") as f:json.dump(report,f,indent=2)
    print("[B04]",name,tris,"tris",report["size_m"])
    return report


def proxy_geometry():
    """Original rough Manny A-pose proxy, soles Z=0, crown Z=1.35m."""
    g=b.Geometry()
    # Lower body: chunky neutral mannequin, not a game character asset.
    g.box((-.005,0,1.005),(.235,.32,.19),"Metal",bevel=.025)
    g.loft([ellipse_ring(.115,.16,1.06,n=8),ellipse_ring(.115,.19,1.26,n=8),
            ellipse_ring(.125,.215,1.45,n=8),ellipse_ring(.08,.15,1.50,n=8)],"Metal")
    g.cylinder((0,0,1.46),(0,0,1.60),.057,"Metal",sides=8)
    g.loft([ellipse_ring(.065,.062,1.56,n=8),ellipse_ring(.107,.090,1.64,n=8),
            ellipse_ring(.105,.090,1.75,n=8),ellipse_ring(.065,.060,1.80,n=8)],"Metal")
    # Nose and eyes make the forward direction obvious for beard fitting.
    g.box((.115,0,1.685),(.055,.034,.040),"Metal",bevel=.006)
    for side in [-1,1]:
        g.box((.102,side*.045,1.713),(.012,.018,.012),"Black")
        g.beam((0,side*.085,.965),(.005,side*.085,.58),.14,.15,"Metal")
        g.beam((.005,side*.085,.58),(.005,side*.085,.14),.105,.12,"Metal")
        g.box((.045,side*.085,.055),(.23,.125,.11),"Metal",bevel=.015)
        g.beam((0,side*.22,1.455),(0,side*.39,1.18),.11,.12,"Metal")
        g.beam((0,side*.39,1.18),(.05,side*.47,.985),.085,.085,"Metal")
        g.box((.05,side*.47,.94),(.07,.075,.105),"Metal",bevel=.012)
    assert abs(min(v[2] for v in g.vertices))<1e-6
    assert abs(max(v[2] for v in g.vertices)-1.8)<1e-6
    g.vertices=[tuple(v*TARGET_SCALE for v in xyz) for xyz in g.vertices]
    return g
