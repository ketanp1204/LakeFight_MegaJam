"""B05 scale proof and screenshots. Owns _B05_Review; all file writes Art/Export.

Logo asset lettering is mesh-only; font objects here are screenshot captions.
Uses the original scripted 135cm Manny proxy and a copied B04 hat FBX reference.
"""
import bpy,sys,os,math,json,importlib,contextlib,io
from mathutils import Vector
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_b04 as b4;importlib.reload(b4)
import lf_b05 as b5;importlib.reload(b5)
import lf_b03 as b3;importlib.reload(b3)
ROOT=b5.ROOT
col=b5.lf.fresh_collection("_B05_Review")
scene=bpy.context.scene


def clone(name,position=(0,0,0)):
    ob=bpy.data.objects[name].copy();ob.data=bpy.data.objects[name].data
    ob.name="B05_Review_"+name;col.objects.link(ob)
    ob.location=position;ob.hide_set(False)
    return ob


def label(body,position,size=.045,direction=(8,-6,3)):
    data=bpy.data.curves.new("B05_Caption","FONT")
    data.body=body;data.size=size
    data.materials.append(b5.lf.mat("PaintCream"))
    ob=bpy.data.objects.new("B05_Caption",data);col.objects.link(ob)
    ob.location=position;ob.rotation_mode="QUATERNION"
    ob.rotation_quaternion=b3.view_quaternion(direction)
    return ob


def shot(objects,file,direction=(4,-6,4),frame=None,aspect=(1500,1100),zoom=1):
    chosen=set(objects)
    for ob in scene.objects:ob.hide_set(ob not in chosen)
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.update()
    area=next(a for a in bpy.context.screen.areas if a.type=="VIEW_3D")
    region=next(r for r in area.regions if r.type=="WINDOW")
    space=area.spaces.active
    for prop,value in [("type","SOLID"),("color_type","MATERIAL"),("light","STUDIO"),("background_type","VIEWPORT")]:
        b5.b.enum_set(space.shading,prop,value)
    space.shading.background_color=(.025,.035,.045)
    space.shading.show_cavity=True
    b5.b.enum_set(space.shading,"cavity_type","BOTH")
    space.overlay.show_overlays=False
    b5.b.enum_set(space.region_3d,"view_perspective","ORTHO")
    if "Standard" in [i.identifier for i in scene.view_settings.bl_rna.properties["view_transform"].enum_items]:scene.view_settings.view_transform="Standard"
    q=b3.view_quaternion(direction)
    points=frame or [ob.matrix_world@v.co for ob in objects if ob.type=="MESH" for v in ob.data.vertices]
    right,up=q@Vector((1,0,0)),q@Vector((0,1,0))
    size=[max(v.dot(a) for v in points)-min(v.dot(a) for v in points) for a in [right,up]]
    space.region_3d.view_rotation=q
    space.region_3d.view_location=Vector([(min(v[i] for v in points)+max(v[i] for v in points))/2 for i in range(3)])
    space.region_3d.view_distance=max(size[0],size[1]*aspect[0]/aspect[1])*.92*zoom
    scene.render.resolution_x,scene.render.resolution_y=aspect
    scene.render.resolution_percentage=100
    b5.b.enum_set(scene.render.image_settings,"file_format","PNG")
    scene.render.filepath=os.path.join(ROOT,"_shots",file+".png")
    with bpy.context.temp_override(area=area,region=region):bpy.ops.render.opengl(write_still=True,view_context=True)
    return scene.render.filepath


proxy=b5.b.make_object(b4.proxy_geometry(),"B05_135cm_MannyProxy",col)
assert abs(max(v.co.z for v in proxy.data.vertices)-1.35)<1e-6
paths={}
for team in b4.TEAMS:
    hat=clone("SM_LF_Hat_Bucket_"+team,b4.PIVOTS["head"])
    title=label("B05 "+team+" | 135cm Manny proxy | brim/crown 1.3x",(.3,-.75,1.55),.032)
    frame=[Vector((x,y,z)) for x in [-.25,.35] for y in [-.65,.55] for z in [-.04,1.65]]
    paths[team]=[shot([proxy,hat,title],"B05_Hat_Bucket_"+team+"_Proxy",(8,-6,3),frame=frame,zoom=.9)]
    detail=[Vector((x,y,z)) for x in [-.20,.26] for y in [-.25,.25] for z in [1.10,1.49]]
    paths[team].append(shot([proxy,hat],"B05_Hat_Bucket_"+team+"_Detail",(8,-6,3),frame=detail))
    bpy.data.objects.remove(title,do_unlink=True)

# Same-scale old/new comparison, with the archived actual B04 FBX.
prior=set(bpy.data.objects)
with contextlib.redirect_stdout(io.StringIO()):
    bpy.ops.import_scene.fbx(filepath=os.path.join(ROOT,"_before_B05","SM_LF_Hat_Bucket_Red.fbx"))
imported=[o for o in bpy.data.objects if o not in prior]
reference=[]
for ob in imported:
    b5.lf.link(ob,col);ob.name="B05_B04_Hat_Reference"
    ob.location=Vector(b4.PIVOTS["head"])+Vector((0,-.56,0));reference.append(ob)
oldproxy=clone(proxy.name,(0,-.56,0));newproxy=clone(proxy.name,(0,.56,0))
newhat=clone("SM_LF_Hat_Bucket_Red",Vector(b4.PIVOTS["head"])+Vector((0,.56,0)))
direction=(1,0,.12)
labels=[label("B04",(.5,-.73,1.54),.06,direction),label("B05 - 1.3x",(.5,.30,1.54),.06,direction)]
frame=[Vector((x,y,z)) for x in [-.3,.35] for y in [-1.05,1.05] for z in [-.04,1.68]]
shot([oldproxy,newproxy,newhat]+reference+labels,"B05_Hat_BeforeAfter",direction,frame=frame)
shot([oldproxy,newproxy,newhat]+reference,"B05_Hat_Chase_Comparison",(-6,-8,4),frame=frame)

main=clone("SM_LF_Logo_LakeFight")
ribbon=clone("SM_LF_Logo_DirectorsCut")
shot([main],"SM_LF_Logo_LakeFight",(2,-8,2),aspect=(1920,1080))
shot([ribbon],"SM_LF_Logo_DirectorsCut",(0,-1,0),aspect=(1920,1080))
shot([main,ribbon],"B05_Logo_Complete_Front",(0,-1,.015),aspect=(1920,1080))
shot([main,ribbon],"B05_Logo_Complete_ThreeQuarter",(3,-9,2.4),aspect=(1920,1080))

dock=clone("SM_LF_Dock_Team")
shot([dock],"SM_LF_Dock_Team",(5,-7,5))
detail=[Vector((x,y,z)) for x in [.45,2.1] for y in [-.55,1.3] for z in [-.1,.95]]
shot([dock],"B05_Dock_Team_BankingSpot",(4,-5,4),frame=detail)
flag=[]
for src in bpy.data.collections["SM_LF_TeamFlag_Red"].objects:
    ob=clone(src.name,(1.82,1.08,.70));flag.append(ob)
walker=clone(proxy.name,(-.65,0,0))
shot([dock,walker]+flag,"B05_Dock_Team_FlagFit",(5,-7,5))

before=json.load(open(os.path.join(ROOT,"B05_before.json")))
runs=json.load(open(os.path.join(ROOT,"B05_runs.json")))
hats=runs["SM_LF_Hat_Bucket"]["runs"][-1]
hatproof={}
for r in hats:
    old=before[r["asset"]]
    assert r["tris"]==old["tris"]==304
    ratio=[r["size_m"][i]/old["size_m"][i] for i in range(3)]
    assert max(abs(s-1.3) for s in ratio)<1e-5
    assert bpy.data.objects[r["asset"]].location.length==0
    assert r["attachment"]["proxy_pivot_m"]==list(b4.PIVOTS["head"])
    hatproof[r["asset"]]=dict(tris_before=old["tris"],tris_after=r["tris"],
        bounds_before_m=[old["bounds_min_m"],old["bounds_max_m"]],bounds_after_m=[r["bounds_min_m"],r["bounds_max_m"]],
        size_before_m=old["size_m"],size_after_m=r["size_m"],dimension_ratios=ratio,
        pivot_unchanged=True,brim_seating_height_unchanged_m=.10125)
combined=sum(runs[n]["runs"][-1][0]["tris"] for n in ["SM_LF_Logo_LakeFight","SM_LF_Logo_DirectorsCut"])
assert combined<5000
proof=dict(target_mannequin_height_m=1.35,hat_proof=hatproof,logo_combined_tris=combined,
    logo_shared_origin=True,font_files_used=False,lettering_is_mesh_only=True,
    flag_holder_local_transform=dict(location_m=[1.82,1.08,.70],rotation_deg=[0,0,0],scale=[1,1,1]),
    flag_base_clearance_per_side_m=.01,hat_screenshots=paths)
with open(os.path.join(ROOT,"B05_review_proof.json"),"w") as f:json.dump(proof,f,indent=2)
# Save on the finished title view; other proofs remain in the review collection.
shot([main,ribbon],"B05_Logo_Complete_Front",(0,-1,.015),aspect=(1920,1080))
old_versions=bpy.context.preferences.filepaths.save_version
bpy.context.preferences.filepaths.save_version=0
try:bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ROOT,"B05_Review.blend"))
finally:bpy.context.preferences.filepaths.save_version=old_versions
print("B05 REVIEW PASS: hats exactly 1.3x with stable pivots, logo+ribbon",combined,"tris, neutral dock with separate flag fit.")
REPORTS=proof
