"""B10 Blender 5.1 render stages; asset exports are never altered by staging."""
import bpy, math, json, hashlib, struct
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1]; OUT=ROOT/'Export'

def scene_new(name):
    old=bpy.data.scenes.get(name)
    if old:
        for ob in list(old.objects): bpy.data.objects.remove(ob,do_unlink=True)
        bpy.data.scenes.remove(old)
    scene=bpy.data.scenes.new(name); bpy.context.window.scene=scene
    scene.unit_settings.system='METRIC'; scene.unit_settings.scale_length=1
    return scene

def clone(scene,name,position=(0,0,0)):
    src=bpy.data.objects[name]; ob=bpy.data.objects.new('B10_Review_'+name,src.data)
    scene.collection.objects.link(ob); ob.location=position; return ob

def camera(scene,target,offset,scale):
    data=bpy.data.cameras.new('B10_Ortho'); ob=bpy.data.objects.new('B10_Ortho',data)
    scene.collection.objects.link(ob); ob.location=Vector(target)+Vector(offset)
    ob.rotation_euler=(Vector(target)-ob.location).to_track_quat('-Z','Y').to_euler()
    data.type='ORTHO'; data.ortho_scale=scale; data.clip_end=3000; scene.camera=ob

def light(scene,icon=False):
    if icon:
        for pos,power,size in [((-3,-4,6),450,5),((4,-2,2),180,4)]:
            data=bpy.data.lights.new('B10_Softbox','AREA'); data.energy=power; data.shape='DISK'; data.size=size
            ob=bpy.data.objects.new('B10_Softbox',data); scene.collection.objects.link(ob); ob.location=pos
            ob.rotation_euler=(-ob.location).to_track_quat('-Z','Y').to_euler()
    else:
        data=bpy.data.lights.new('B10_Sun','SUN'); data.energy=2.2; data.angle=math.radians(12)
        ob=bpy.data.objects.new('B10_Sun',data); scene.collection.objects.link(ob); ob.rotation_euler=(math.radians(28),math.radians(-22),math.radians(-25))
    world=bpy.data.worlds.get('B10_World') or bpy.data.worlds.new('B10_World'); world.use_nodes=True
    bg=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND')
    bg.inputs['Color'].default_value=(.65,.72,.85,1); bg.inputs['Strength'].default_value=.35
    scene.world=world

def render(scene,path,w=1200,h=800,alpha=False):
    scene.render.engine='BLENDER_EEVEE'
    scene.render.resolution_x=w; scene.render.resolution_y=h; scene.render.resolution_percentage=100
    scene.render.film_transparent=alpha; scene.render.image_settings.file_format='PNG'
    scene.render.image_settings.color_mode='RGBA'; scene.render.image_settings.color_depth='8'
    scene.render.use_file_extension=True; scene.render.use_stamp=False
    scene.view_settings.view_transform='Standard'; scene.view_settings.look='None'; scene.view_settings.exposure=0; scene.view_settings.gamma=1
    path.parent.mkdir(parents=True,exist_ok=True); scene.render.filepath=str(path)
    bpy.ops.render.render(write_still=True)
    # Drop timestamp/duration text chunks, retain compressed pixels exactly.
    raw=path.read_bytes(); clean=raw[:8]; i=8
    while i<len(raw):
        n=struct.unpack('>I',raw[i:i+4])[0]; end=i+n+12
        if raw[i+4:i+8]!=b'tEXt': clean+=raw[i:end]
        i=end
    path.write_bytes(clean)
    return hashlib.sha256(clean).hexdigest()

def icon(name):
    scene=scene_new('B10_UI_'+name); clone(scene,name)
    camera(scene,(0,0,0),(0,-6,0),2.75 if name.endswith('Paddle') else 2.55)
    light(scene,True)
    return render(scene,OUT/'UI'/(name+'.png'),256,256,True)

def screenshots():
    scene=scene_new('B10_Ground_Review')
    clone(scene,'SM_LF_GroundPatch_01',(-20,0,0)); clone(scene,'SM_LF_GroundPatch_02',(20,0,0))
    camera(scene,(0,0,.3),(60,-95,85),100); light(scene)
    render(scene,OUT/'_shots'/'B10_ground_tiles.png',1400,850)
    scene=scene_new('B10_Hills_Review')
    for name,x in [('SM_LF_Hill_Far_01',-180),('SM_LF_Hill_Far_02',-15),('SM_LF_Hill_Far_03',180)]: clone(scene,name,(x,0,0))
    camera(scene,(0,0,13),(120,-700,310),620); light(scene)
    render(scene,OUT/'_shots'/'B10_distant_hills.png',1500,650)
    scene=scene_new('B10_ShoreProps_Review')
    for name,pos in [('SM_LF_Bush_01',(-2.8,0,0)),('SM_LF_Bush_02',(-.7,0,0)),
                     ('SM_LF_Log',(2,0,0)),('SM_LF_Fence_Section',(0,2.6,0))]: clone(scene,name,pos)
    camera(scene,(0,.6,.55),(7,-13,7),9.5); light(scene)
    render(scene,OUT/'_shots'/'B10_shore_props.png',1400,950)
    scene=scene_new('B10_TreeCluster_Review'); clone(scene,'SM_LF_TreeCluster_01')
    camera(scene,(0,0,3.2),(17,-28,17),20); light(scene)
    render(scene,OUT/'_shots'/'B10_tree_cluster.png',1200,900)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'B10_Review.blend'))
    print('Four screenshots and B10_Review.blend saved.')
