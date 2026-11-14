"""Blender-only B11 review renders. Staging never changes exported asset coordinates."""
import sys, math, json
from pathlib import Path
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1]; OUT=ROOT/'Export'
sys.path.insert(0,str(ROOT/'BlenderScripts'))
import lf_b11 as b
import B10_Review as r
import lf_common as lf

def flying_pose(scene,pos,angle):
    body=r.clone(scene,'SM_LF_Bird_Body',pos)
    for side in ['L','R']:
        wing=r.clone(scene,'SM_LF_Bird_Wing_'+side)
        wing.parent=body; wing.location=b.SHOULDERS[side]
        wing.rotation_euler.x=math.radians(-angle if side=='L' else angle)
    return body

def flying_screenshots():
    scene=new('B11_FlyingBirds_Review')
    for pos,angle in [((-1.45,0,.25),42),((0,0,.25),0),((1.45,0,.25),-32)]: flying_pose(scene,pos,angle)
    copy_asset(scene,'SM_LF_Bird_Glide',(0,-1.35,.25))
    r.camera(scene,(0,-.40,.25),(2.5,-4,3.4),4.65)
    r.render(scene,OUT/'_shots'/'B11_flying_bird_poses.png',1600,1100)
    # Frontal view makes shoulder rotation and silhouette easy to compare.
    scene=new('B11_FlyingBirds_Front_Review')
    for pos,angle in [((0,-1.32,0),42),((0,0,0),0),((0,1.32,0),-32)]: flying_pose(scene,pos,angle)
    copy_asset(scene,'SM_LF_Bird_Glide',(0,0,-.94))
    r.camera(scene,(0,0,-.25),(4,0,.55),4.35)
    r.render(scene,OUT/'_shots'/'B11_flying_bird_front.png',1600,850)
    print('Saved flying birds: up / level / down plus merged glide, staged only.')

def insect_screenshots():
    scene=new('B11_Insects_Review')
    body=r.clone(scene,'SM_LF_Dragonfly',(-.18,0,0))
    wings=r.clone(scene,'SM_LF_Dragonfly_Wings'); wings.parent=body; wings.location=(.0255,0,0)
    for x,angle in [(0,12),(.18,55)]:
        body=r.clone(scene,'SM_LF_Butterfly_Body',(x,0,0))
        for side in ['L','R']:
            wing=r.clone(scene,'SM_LF_Butterfly_Wing_'+side); wing.parent=body; wing.location=b.BUTTERFLY_HINGES[side]
            wing.rotation_euler.x=math.radians(-angle if side=='L' else angle)
    r.camera(scene,(0,0,0),(.12,-.30,.60),.61)
    r.render(scene,OUT/'_shots'/'B11_insects.png',1600,850)
    print('Saved dragonfly and butterfly open / raised-wing assemblies, authored at 1.5 scale.')

def new(name):
    scene=r.scene_new(name)
    r.light(scene)
    # Sky blue is world illumination/background, not an added mesh material.
    world=bpy.data.worlds.get('B11_Sky') or bpy.data.worlds.new('B11_Sky'); world.use_nodes=True
    bg=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND')
    bg.inputs['Color'].default_value=(.22,.39,.62,1); bg.inputs['Strength'].default_value=.6
    scene.world=world
    return scene

def copy_asset(scene,name,location=(0,0,0)):
    col=bpy.data.collections[name]; copies={}
    for src in col.objects:
        ob=bpy.data.objects.new('B11_Review_'+src.name,src.data); scene.collection.objects.link(ob)
        ob.location=src.location; ob.rotation_euler=src.rotation_euler; copies[src]=ob
    for src,ob in copies.items():
        if src.parent in copies: ob.parent=copies[src.parent]
        else: ob.location+=Vector(location)
    return copies

def water(scene,size=5):
    me=bpy.data.meshes.new('B11_ReviewWater')
    me.from_pydata([(-size,-size,-.002),(size,-size,-.002),(size,size,-.002),(-size,size,-.002)],[],[(0,1,2,3)])
    me.materials.append(lf.mat('Cloth'))
    ob=bpy.data.objects.new('B11_ReviewWater',me); scene.collection.objects.link(ob)
    return ob

def screenshots():
    scene=new('B11_Clouds_Review')
    for name,pos in [('SM_LF_Cloud_01',(-43,0,76)),('SM_LF_Cloud_02',(43,0,70)),
                     ('SM_LF_Cloud_03',(-43,0,4)),('SM_LF_Cloud_04',(43,0,0))]: copy_asset(scene,name,pos)
    r.camera(scene,(0,0,53),(26,-250,45),174)
    r.render(scene,OUT/'_shots'/'B11_clouds.png',1500,1100)
    scene=new('B11_CloudUndersides_Review')
    for i in range(4): copy_asset(scene,'SM_LF_Cloud_0'+str(i+1),((i-1.5)*65,0,30))
    r.camera(scene,(0,0,40),(20,-250,-90),290)
    r.render(scene,OUT/'_shots'/'B11_cloud_undersides.png',1600,650)
    scene=new('B11_Birds_Review')
    for name,pos in [('SM_LF_Duck',(-.74,-.12,0)),('SM_LF_Swan',(-.10,0,0)),('SM_LF_Heron',(.66,.13,0))]: copy_asset(scene,name,pos)
    water(scene)
    r.camera(scene,(.06,0,.44),(1.1,-4,1.2),2.20)
    r.render(scene,OUT/'_shots'/'B11_birds.png',1500,1100)
    scene=new('B11_Junk_Review')
    for name,pos in [('SM_LF_Barrel',(-.78,.12,0)),('SM_LF_LifeRing',(.48,.22,0)),('SM_LF_Bottle_Message',(-.15,-.68,0))]: copy_asset(scene,name,pos)
    water(scene)
    r.camera(scene,(-.08,0,.02),(1.1,-3.5,3.2),2.70)
    r.render(scene,OUT/'_shots'/'B11_floating_junk.png',1500,1100)
    scene=new('B11_Swan_Pivot_Review')
    copies=copy_asset(scene,'SM_LF_Swan',(-.48,0,0))
    posed=copy_asset(scene,'SM_LF_Swan',(.48,0,0))
    head=next(ob for src,ob in posed.items() if src.name.endswith('HeadNeck'))
    head.rotation_euler.y=math.radians(32); head.rotation_euler.z=math.radians(-15)
    water(scene)
    r.camera(scene,(.10,0,.28),(.4,-3,1.0),2.40)
    r.render(scene,OUT/'_shots'/'B11_swan_neck_pose.png',1400,850)
    bpy.context.window.scene=bpy.data.scenes['B11_Birds_Review']
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'B11_Review.blend'))
    print('Saved five B11 screenshots and B11_Review.blend. Swan pose is staging only.')
