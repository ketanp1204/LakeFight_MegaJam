"""Reproducible B03 review scene, screenshot fixtures, boat-occlusion and seam proof.

All staging geometry is screenshot-only, in _B03_Review. No FBX is exported.
The rowboat is this jam repo's original B01 mesh, used only as a scale reference.
Grey palette Metal boxes represent an opaque waterline, not a new water asset.
"""
import bpy, sys, os, json, math, contextlib, io, importlib
from mathutils import Vector
from mathutils.bvhtree import BVHTree
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_b03 as b3
importlib.reload(b3)
lf=b3.lf
ROOT=b3.b.ROOT
col=lf.fresh_collection("_B03_Review")


def clone(name,location=(0,0,0),angle=0):
    src=bpy.data.objects[name]
    ob=src.copy()
    ob.data=src.data
    ob.name="Review_"+name
    col.objects.link(ob)
    ob.location=location
    ob.rotation_euler=(0,0,angle)
    ob.hide_set(False)
    return ob


def label(text,position,size=.16):
    data=bpy.data.curves.new("Review_Label","FONT")
    data.body=text
    data.size=size
    data.materials.append(lf.mat("PaintCream"))
    ob=bpy.data.objects.new("Review_Label",data)
    col.objects.link(ob)
    ob.location=position
    ob.rotation_euler=(math.pi/2,0,0)
    return ob


def proxy(centre,size):
    g=b3.b.Geometry()
    g.box(centre,size,"Metal")
    return b3.b.make_object(g,"Review_WaterlineProxy",col)


def snapshot(objects,filename,direction=(4,-6,4),frame=None):
    scene=bpy.context.scene
    chosen=set(objects)
    for ob in scene.objects: ob.hide_set(ob not in chosen)
    bpy.ops.object.select_all(action="DESELECT")
    area=next(a for a in bpy.context.screen.areas if a.type=="VIEW_3D")
    region=next(r for r in area.regions if r.type=="WINDOW")
    space=area.spaces.active
    b3.b.enum_set(space.shading,"type","SOLID")
    b3.b.enum_set(space.shading,"color_type","MATERIAL")
    b3.b.enum_set(space.shading,"light","STUDIO")
    b3.b.enum_set(space.shading,"background_type","VIEWPORT")
    space.shading.background_color=(.055,.065,.08)
    space.shading.show_cavity=True
    b3.b.enum_set(space.shading,"cavity_type","BOTH")
    space.overlay.show_overlays=False
    b3.b.enum_set(space.region_3d,"view_perspective","ORTHO")
    q=b3.view_quaternion(direction)
    bpy.context.view_layer.update()
    coords=frame or [ob.matrix_world@v.co for ob in objects if ob.type=="MESH" for v in ob.data.vertices]
    right,up=q@Vector((1,0,0)),q@Vector((0,1,0))
    extents=[max(v.dot(a) for v in coords)-min(v.dot(a) for v in coords) for a in [right,up]]
    space.region_3d.view_rotation=q
    space.region_3d.view_location=Vector([(min(v[i] for v in coords)+max(v[i] for v in coords))/2 for i in range(3)])
    space.region_3d.view_distance=max(extents[0],extents[1]*1.5)*1.02
    scene.render.resolution_x,scene.render.resolution_y=1500,1000
    scene.render.resolution_percentage=100
    b3.b.enum_set(scene.render.image_settings,"file_format","PNG")
    scene.render.filepath=os.path.join(ROOT,"_shots",filename+".png")
    bpy.context.view_layer.update()
    with bpy.context.temp_override(area=area,region=region): bpy.ops.render.opengl(write_still=True,view_context=True)
    return scene.render.filepath


bed=clone("SM_LF_ReedBed")
prior=set(bpy.data.objects)
with contextlib.redirect_stdout(io.StringIO()): bpy.ops.import_scene.fbx(filepath=os.path.join(ROOT,"SM_LF_Rowboat.fbx"))
imported=[o for o in bpy.data.objects if o not in prior]
boat=[]
for ob in imported:
    if ob.type!="MESH" or ob.name.startswith("UCX_"):
        bpy.data.objects.remove(ob,do_unlink=True)
        continue
    lf.link(ob,col)
    ob.name="Review_B01_Rowboat"
    ob.location.y+=2.4
    boat.append(ob)
water=proxy((0,0,-.42),(5.,8.,.84))
title=label("B03 REED BED | 3,468 tris | 30 cattails",(-2.35,-4.1,2.15),.14)
frame=[Vector((x,0,z)) for x in [-2.5,2.5] for z in [-.25,2.4]]
snapshot(boat+[water,title],"B03_ReedBed_Boat_Unoccluded",(0,-1,0),frame)
snapshot([bed]+boat+[water,title],"SM_LF_ReedBed_HidingBoat",(0,-1,0),frame)
# Shoot through the actual above-water boat silhouette, not just its rectangle.
bpy.context.view_layer.update()
dg=bpy.context.evaluated_depsgraph_get()
bedtree=BVHTree.FromObject(bed,dg)
boattrees=[(ob,BVHTree.FromObject(ob,dg)) for ob in boat]
covered=total=0
for j in range(241):
    for k in range(61):
        start=Vector((-1.61+j*3.22/240,-5.,.004+k*.55/60))
        hits=[]
        for ob,tree in boattrees:
            # BVHTree.FromObject works in local coordinates.
            inv=ob.matrix_world.inverted()
            hit=tree.ray_cast(inv@start,Vector((0,1,0)),12.)
            if hit[0] is not None: hits.append((ob.matrix_world@hit[0]-start).length)
        if hits:
            total+=1
            reedhit=bedtree.ray_cast(start,Vector((0,1,0)),min(hits))
            covered+=reedhit[0] is not None
boatproof=dict(covered=covered,boat_silhouette_samples=total,fraction=covered/total,
               window="X=-1.61..1.61m, Z=.004.. .554m, 241x61 rays; actual original B01 rowboat at Y=2.4m; submerged portion excluded")
print("ACTUAL BOAT OCCLUSION",boatproof)

# Place full sloped bank modules in two joined fixtures.
fixtures=[]
for kind,offset in [("CornerOut",0),("CornerIn",16)]:
    corner=clone("SM_LF_Shore_"+kind,(offset,0,0))
    if kind=="CornerOut":
        a=clone("SM_LF_Shore_Straight",(offset+4,-4,0),math.pi/2)
        c=clone("SM_LF_Shore_Straight",(offset-4,4,0),math.pi)
    else:
        a=clone("SM_LF_Shore_Straight",(offset+4,-4,0),-math.pi/2)
        c=clone("SM_LF_Shore_Straight",(offset-4,4,0),0)
    fixtures.extend([corner,a,c])
bpy.context.view_layer.update()
gaps=[]
for i in [0,3]:
    corner,a,c=fixtures[i:i+3]
    cv=[corner.matrix_world@v.co for v in corner.data.vertices]
    for straight in [a,c]:
        sv=[straight.matrix_world@v.co for v in straight.data.vertices]
        # Seven points at the shared profile, all must coincide.
        distances=sorted(min((p-q).length for q in cv) for p in sv)
        gaps.append(max(distances[:7]))
assert max(gaps)<1e-5,gaps
snapshot(fixtures,"B03_Shore_Modular_FullSlope",(5,-8,7))
bankwater=proxy((8,-1,-.42),(40.,32.,.84))
snapshot(fixtures+[bankwater],"B03_Shore_Modular_Waterline",(5,-8,7))

stump=clone("SM_LF_Stump",(-8,-4,0))
stumpwater=proxy((-8,-4,-.42),(3.2,3.2,.84))
snapshot([stump,stumpwater],"SM_LF_Stump_Waterline",(4,-6,3))

# Keep a spaced gallery in the saved .blend. Asset masters remain at original origins.
gallery=fixtures+[bed]+boat+[water]
gallery += [clone("SM_LF_Island_S",(-8,10,0)),clone("SM_LF_Island_M",(7,19,0)),
            clone("SM_LF_LilyPads",(-6,1,0)),stump,
            clone("SM_LF_Tree_Pine",(-8,5,0)),clone("SM_LF_Tree_Round",(-4,7,0))]
snapshot(gallery,"B03_Gallery",(5,-8,8))
proof=dict(actual_boat_occlusion=boatproof,shore_seam_max_gaps_m=gaps,
           shore_fixtures="Out: corner origin; straights (4,-4) Rz90,(-4,4) Rz180. In: corner (16,0); straights (20,-4) Rz-90,(12,4) Rz0.")
with open(os.path.join(ROOT,"B03_review_proof.json"),"w") as f: json.dump(proof,f,indent=2)
previous=bpy.context.preferences.filepaths.save_version
bpy.context.preferences.filepaths.save_version=0
try: bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ROOT,"B03_Review.blend"))
finally: bpy.context.preferences.filepaths.save_version=previous
print("REVIEW SAVED; seam maximum gap",max(gaps),"m")
REPORTS=proof
