"""B06 wireframe evidence, derived directly from delivered UCX edges.

Only _B06_Review staging is created. UCX objects stay material-free; cream/grey
curve overlays and captions are screenshot-only and never exported to FBX.
"""
import bpy,sys,os,json,contextlib,io,re
from mathutils import Vector
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf
import lf_b03 as b3
import lf_b02 as b
ROOT=b.ROOT
# Drop only this task's obsolete prototype staging, never asset collections.
design=bpy.data.collections.get("_B06_Design")
if design:
    for ob in list(design.objects):bpy.data.objects.remove(ob,do_unlink=True)
    bpy.data.collections.remove(design)
col=lf.fresh_collection("_B06_Review")
scene=bpy.context.scene
audit=json.load(open(os.path.join(ROOT,"B06_fbx_audit.json")))


def edges(ob,name):
    curve=bpy.data.curves.new(name,"CURVE")
    b.enum_set(curve,"dimensions","3D")
    curve.bevel_depth=.0016;curve.bevel_resolution=0;curve.resolution_u=1
    curve.materials.append(lf.mat("Metal" if re.sub(r"\.\d+$","",ob.name).endswith(("_00","_05")) else "PaintCream"))
    for edge in ob.data.edges:
        spline=curve.splines.new("POLY");spline.points.add(1)
        for point,index in zip(spline.points,edge.vertices):
            p=ob.matrix_world@ob.data.vertices[index].co;point.co=(*p,1)
    wire=bpy.data.objects.new(name,curve);col.objects.link(wire)
    wire.show_in_front=True
    return wire


def label(body,position,q,size):
    data=bpy.data.curves.new("B06_Caption","FONT");data.body=body;data.size=size
    data.materials.append(lf.mat("PaintCream"))
    ob=bpy.data.objects.new("B06_Caption",data);col.objects.link(ob)
    ob.location=position;ob.rotation_mode="QUATERNION";ob.rotation_quaternion=q
    return ob


def shot(objects,title,subtitle,filename,direction):
    q=b3.view_quaternion(direction)
    right,up,eye=[q@Vector(v) for v in [(1,0,0),(0,1,0),(0,0,1)]]
    top=abs(eye.z)>.9
    centre=Vector((0,0,.15))
    width,height=(3.60,2.05) if top else (1.90,1.08)
    captions=[label(title,centre-right*(width/2-.05)+up*(height/2-.10)+eye*.5,q,.07 if top else .045),
              label(subtitle,centre-right*(width/2-.05)-up*(height/2-.03)+eye*.5,q,.045 if top else .028)]
    chosen=set(objects+captions)
    for ob in scene.objects:ob.hide_set(ob not in chosen)
    bpy.ops.object.select_all(action="DESELECT")
    area=next(a for a in bpy.context.screen.areas if a.type=="VIEW_3D")
    region=next(r for r in area.regions if r.type=="WINDOW")
    space=area.spaces.active
    for prop,value in [("type","SOLID"),("color_type","MATERIAL"),("light","STUDIO"),("background_type","VIEWPORT")]:b.enum_set(space.shading,prop,value)
    space.shading.background_color=(.025,.035,.045)
    space.shading.show_cavity=True;b.enum_set(space.shading,"cavity_type","BOTH")
    space.overlay.show_overlays=True;space.overlay.show_floor=False
    space.overlay.show_cursor=False
    space.overlay.show_axis_x=False;space.overlay.show_axis_y=False
    b.enum_set(space.region_3d,"view_perspective","ORTHO")
    space.region_3d.view_rotation=q;space.region_3d.view_location=centre
    space.region_3d.view_distance=max(width,height*16/9)*.90
    scene.render.resolution_x=1920;scene.render.resolution_y=1080;scene.render.resolution_percentage=100
    b.enum_set(scene.render.image_settings,"file_format","PNG")
    scene.render.filepath=os.path.join(ROOT,"_shots",filename+".png")
    bpy.context.view_layer.update()
    with bpy.context.temp_override(area=area,region=region):bpy.ops.render.opengl(write_still=True,view_context=True)
    for ob in captions:bpy.data.objects.remove(ob,do_unlink=True)
    return scene.render.filepath


paths=[];variants={}
for team in ["Red","Blue","Green","Yellow"]:
    name="SM_LF_Rowboat"+("" if team=="Red" else "_"+team)
    src=bpy.data.objects[name]
    hull=src.copy();hull.data=src.data;hull.name="B06_Review_"+name;col.objects.link(hull)
    wires=[edges(bpy.data.objects[f"UCX_{name}_{i:02d}"],f"B06_Wire_{team}_{i:02d}") for i in range(12)]
    objects=[hull]+wires;variants[team]=objects
    gap=audit[name]["fit"]["max_absolute_gap_m"]*100
    text=f"12 convex pieces | max 14 vertices | outline gap {gap:.3f} cm | grey floor/stern unchanged"
    paths.append(shot(objects,"B06 UCX - "+team+" - TOP",text,"B06_UCX_"+team+"_Top",(0,0,1)))
    paths.append(shot(objects,"B06 UCX - "+team+" - FRONT",text,"B06_UCX_"+team+"_Front",(1,0,0)))

prior=set(bpy.data.objects)
with contextlib.redirect_stdout(io.StringIO()):bpy.ops.import_scene.fbx(filepath=os.path.join(ROOT,"_before_B06","SM_LF_Rowboat.fbx"))
old=[o for o in bpy.data.objects if o not in prior]
for ob in old:lf.link(ob,col)
oldhull=next(o for o in old if not re.sub(r"\.\d+$","",o.name).startswith("UCX_"))
oldwires=[edges(o,"B06_BeforeWire_"+str(i)) for i,o in enumerate(old) if re.sub(r"\.\d+$","",o.name).startswith("UCX_")]
paths.append(shot([oldhull]+oldwires,"BEFORE B06 - TOP","Original six-piece collision; source FBX archive","B06_UCX_Before_Top",(0,0,1)))
paths.append(shot([oldhull]+oldwires,"BEFORE B06 - FRONT","Original six-piece collision; source FBX archive","B06_UCX_Before_Front",(1,0,0)))
for direction,suffix in [((0,0,1),"Top"),((1,0,0),"Front")]:
    paths.append(shot(variants["Red"],"B06 UCX - AFTER - "+suffix.upper(),"12 convex / max14 verts / render unchanged / outline gap 0.879cm","B06_UCX_"+suffix,direction))
old_save=bpy.context.preferences.filepaths.save_version;bpy.context.preferences.filepaths.save_version=0
try:bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ROOT,"B06_Review.blend"))
finally:bpy.context.preferences.filepaths.save_version=old_save
REPORTS=dict(screenshots=paths,ucx_overlay_from_actual_edges=True,source_ucx_materials_untouched=True)
with open(os.path.join(ROOT,"B06_review_proof.json"),"w") as f:json.dump(REPORTS,f,indent=2)
print("B06 wireframe screenshots saved:",len(paths),"plus B06_Review.blend")
