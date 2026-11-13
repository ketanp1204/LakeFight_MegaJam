"""Original 135cm proxy for Unreal Manny at 0.75, B04 fitting screenshots.

Owns only _B04_Review. Uses assumed pivots, not a Manny skeleton import.
All proxy/staging meshes are made here/from lf_b04.py, never exported as game props.
"""
import bpy,sys,os,math,json,importlib
from mathutils import Vector,Matrix
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_b04 as b4;importlib.reload(b4)
import lf_b03 as b3;importlib.reload(b3)
col=b4.lf.fresh_collection("_B04_Review")
scene=bpy.context.scene
ROOT=b4.ROOT
S=b4.TARGET_SCALE


def copy_object(source,name):
    ob=source.copy();ob.data=source.data
    ob.name=name;col.objects.link(ob);ob.hide_set(False)
    return ob


def text(body,position,size=.04,direction=(8,-6,3)):
    data=bpy.data.curves.new("B04_Label","FONT")
    data.body=body;data.size=size
    data.materials.append(b4.lf.mat("PaintCream"))
    ob=bpy.data.objects.new("B04_Label",data);col.objects.link(ob)
    ob.location=position;ob.rotation_mode="QUATERNION"
    ob.rotation_quaternion=b3.view_quaternion(direction)
    return ob


def snapshot(objects,filename,direction=(8,-6,3),frame=None,zoom=1):
    chosen=set(objects)
    for ob in scene.objects:ob.hide_set(ob not in chosen)
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.update()
    area=next(a for a in bpy.context.screen.areas if a.type=="VIEW_3D")
    region=next(r for r in area.regions if r.type=="WINDOW")
    space=area.spaces.active
    for prop,value in [("type","SOLID"),("color_type","MATERIAL"),("light","STUDIO"),("background_type","VIEWPORT")]:
        b4.b.enum_set(space.shading,prop,value)
    space.shading.background_color=(.035,.045,.06)
    space.shading.show_cavity=True
    b4.b.enum_set(space.shading,"cavity_type","BOTH")
    space.overlay.show_overlays=False
    b4.b.enum_set(space.region_3d,"view_perspective","ORTHO")
    if "Standard" in [i.identifier for i in scene.view_settings.bl_rna.properties["view_transform"].enum_items]:
        scene.view_settings.view_transform="Standard"
    q=b3.view_quaternion(direction)
    points=frame or [ob.matrix_world@v.co for ob in objects if ob.type=="MESH" for v in ob.data.vertices]
    right,up=q@Vector((1,0,0)),q@Vector((0,1,0))
    extents=[max(v.dot(a) for v in points)-min(v.dot(a) for v in points) for a in [right,up]]
    space.region_3d.view_rotation=q
    space.region_3d.view_location=Vector([(min(v[i] for v in points)+max(v[i] for v in points))/2 for i in range(3)])
    space.region_3d.view_distance=max(extents[0],extents[1]*1.25)*1.03*zoom
    scene.render.resolution_x,scene.render.resolution_y=1500,1200
    scene.render.resolution_percentage=100
    b4.b.enum_set(scene.render.image_settings,"file_format","PNG")
    scene.render.filepath=os.path.join(ROOT,"_shots",filename+".png")
    with bpy.context.temp_override(area=area,region=region):bpy.ops.render.opengl(write_still=True,view_context=True)
    return scene.render.filepath


proxy=b4.b.make_object(b4.proxy_geometry(),"B04_135cmProxy_Master",col)
height=max(v.co.z for v in proxy.data.vertices)-min(v.co.z for v in proxy.data.vertices)
assert abs(height-1.35)<1e-6,height
g=b4.b.Geometry()
g.cylinder((0,-.71,0),(0,-.71,1.8),.003,"PaintCream",sides=4)
for z in [0,1.8]:g.beam((-.07,-.71,z),(.07,-.71,z),.005,.005,"PaintCream")
g.vertices=[tuple(v*S for v in xyz) for xyz in g.vertices]
ruler=b4.b.make_object(g,"B04_135cm_Ruler",col)
q=b3.view_quaternion((8,-6,3))
ruler_label=text("135 cm",Vector((0,-.71,.88))*S+(q@Vector((0,0,1)))*.06,.055*S)
raw=json.load(open(os.path.join(ROOT,"B04_runs.json")))
reports=[r for entry in raw.values() for r in entry["runs"][-1]]
all_shots={};gallery=[]
for index,report in enumerate(reports):
    name=report["asset"]
    mannequin=copy_object(proxy,"Fit_Proxy_"+name)
    prop=copy_object(bpy.data.objects[name],"Fit_"+name)
    bone=report["attachment"]["bone"]
    prop.location=b4.PIVOTS[bone]
    if bone=="hand_r":prop.rotation_euler=(0,math.radians(25),0)
    q=b3.view_quaternion((8,-6,3));right=q@Vector((1,0,0));up=q@Vector((0,1,0));eye=q@Vector((0,0,1))
    title=text(name+"\n135cm UE Manny proxy (0.75) | "+bone,(Vector((0,0,.94))-right*.66+up*1.06+eye*.4)*S,.035*S)
    frame=[Vector((x,y,z))*S for x in [-.4,.4] for y in [-.82,.65] for z in [-.06,2.12]]
    if bone=="hand_r":
        frame=[Vector((x,y,z))*S for x in [-.3,1.25] for y in [-.82,.65] for z in [-.06,2.9]]
        title.location=(Vector((.3,0,1.3))-right*.84+up*1.4+eye*.4)*S
    paths=[snapshot([mannequin,prop,ruler,ruler_label,title],name+"_Proxy",frame=frame,zoom=.82)]
    if bone=="head":
        detail=[Vector((x,y,z))*S for x in [-.19,.32] for y in [-.25,.25] for z in [1.44,1.97]]
    elif bone=="spine_03":
        detail=[Vector((x,y,z))*S for x in [-.27,.34] for y in [-.33,.33] for z in [1.05,1.60]]
    else:
        detail=[Vector((x,y,z))*S for x in [-.2,.45] for y in [.25,.65] for z in [.72,1.42]]
    paths.append(snapshot([mannequin,prop],name+"_Detail",frame=detail))
    all_shots[name]=paths
    bpy.data.objects.remove(title,do_unlink=True)
    # Three rows of four dressed proxies; original attachment masters stay at 0.
    move=Vector((-3.*(index//4),1.5*(index%4),0))*S
    mannequin.location+=move;prop.location+=move
    gallery.extend([mannequin,prop])

# One fully dressed leader, verifying that the separate face pieces can coexist.
leader=copy_object(proxy,"Fit_CrewLeader_Proxy")
leaderparts=[leader]
for name in ["SM_LF_Hat_Captain","SM_LF_Beard","SM_LF_Moustache","SM_LF_LifeVest_Blue","SM_LF_FishingRod"]:
    source=next(r for r in reports if r["asset"]==name)
    ob=copy_object(bpy.data.objects[name],"Fit_Leader_"+name)
    ob.location=b4.PIVOTS[source["attachment"]["bone"]]
    if source["attachment"]["bone"]=="hand_r":ob.rotation_euler=(0,math.radians(25),0)
    leaderparts.append(ob)
snapshot(leaderparts+[ruler,ruler_label],"B04_CrewLeader_Proxy")
detail=[Vector((x,y,z))*S for x in [-.22,.34] for y in [-.27,.27] for z in [1.44,1.97]]
snapshot(leaderparts,"B04_CrewLeader_Head",frame=detail)
for ob in leaderparts:ob.location+=Vector((3.,2.2,0))*S
gallery.extend(leaderparts)

# Palette verification lineup: same boat and flag geometry, new team shader colours.
palette_objects=[]
for index,team in enumerate(b4.TEAMS):
    name="SM_LF_Rowboat"+("" if team=="Red" else "_"+team)
    boat=copy_object(bpy.data.objects[name],"Palette_"+name)
    boat.location=(0,index*3.,0)
    palette_objects.append(boat)
    flagcol=bpy.data.collections["SM_LF_TeamFlag_"+team]
    for source in flagcol.objects:
        ob=copy_object(source,"Palette_"+source.name)
        ob.location=(2.,index*3.,0)
        palette_objects.append(ob)
snapshot(palette_objects,"B04_TeamPalette_BoatsFlags",(7,-6,7))
snapshot(gallery,"B04_DressUp_Gallery",(9,-8,7))
evidence=dict(target="UE5 Manny at 0.75 scale",proxy_height_m=height,proxy_is_synthetic=True,assumed_pivots_m=b4.PIVOTS,
              aligned_socket_axes="X forward / Y right / Z up",raw_Manny_fit_verified=False,
              fishing_rod_preview_rotation_about_Y_deg=25,asset_screenshots=all_shots)
with open(os.path.join(ROOT,"B04_review_proof.json"),"w") as f:json.dump(evidence,f,indent=2)
save_version=bpy.context.preferences.filepaths.save_version
bpy.context.preferences.filepaths.save_version=0
try:bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ROOT,"B04_Review.blend"))
finally:bpy.context.preferences.filepaths.save_version=save_version
print("B04 REVIEW",height,"m proxy;",len(all_shots),"assets photographed full-body and detail")
REPORTS=evidence
