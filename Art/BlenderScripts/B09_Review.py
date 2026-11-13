"""B09 serial MCP runner: before/after, two runs, actual FBX invariance and poses."""
import bpy, sys, json, math, hashlib, runpy, contextlib, io
from pathlib import Path
from mathutils import Vector, Matrix
ROOT=Path(__file__).resolve().parents[1]; OUT=ROOT/'Export'
sys.path.insert(0,str(ROOT/'BlenderScripts'))
BOATS=['SM_LF_Rowboat','SM_LF_Rowboat_Blue','SM_LF_Rowboat_Green','SM_LF_Rowboat_Yellow']
def fresh_scene(name):
    old=bpy.data.scenes.get(name)
    if old: bpy.data.scenes.remove(old)
    scene=bpy.data.scenes.new(name);bpy.context.window.scene=scene
    scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
    return scene

def signature(ob):
    me=ob.data;me.calc_loop_triangles()
    mats=[m.name.split('.')[0] for m in me.materials]
    faces=sorted((tuple(sorted(tuple(round(v,4) for v in me.vertices[i].co) for i in p.vertices)),mats[p.material_index] if mats else None) for p in me.polygons)
    payload={'faces':faces,'vertices':sorted(tuple(round(v,4) for v in x.co) for x in me.vertices),'materials':mats,'triangles':len(me.loop_triangles),'origin':[round(v,6) for v in ob.location],'scale':[round(v,6) for v in ob.scale]}
    return hashlib.sha256(json.dumps(payload,sort_keys=True).encode()).hexdigest(),len(me.loop_triangles)

def read_fbx(path):
    bpy.ops.object.select_all(action='DESELECT');bpy.ops.import_scene.fbx(filepath=str(path))
    objs=list(bpy.context.selected_objects)
    meshes={o.name.split('.')[0]:signature(o) for o in objs if o.type=='MESH'}
    sockets=[{'name':o.name.split('.')[0],'parent':o.parent.name.split('.')[0] if o.parent else None,'location_m':[round(x,6) for x in o.location],'rotation':[round(x,6) for x in o.rotation_euler],'scale':[round(x,6) for x in o.scale]} for o in objs if o.type=='EMPTY']
    return objs,{'meshes':meshes,'sockets':sockets}

def render(scene,path,top=False):
    for ob in scene.objects: ob.hide_render=ob.type=='MESH' and ob.name.startswith('UCX_')
    rig=bpy.data.collections.get('B09_RenderRig')
    if rig:
        for ob in list(rig.objects):bpy.data.objects.remove(ob,do_unlink=True)
        bpy.data.collections.remove(rig)
    rig=bpy.data.collections.new('B09_RenderRig');scene.collection.children.link(rig)
    d=bpy.data.cameras.new('B09_Camera');cam=bpy.data.objects.new('B09_Camera',d);rig.objects.link(cam)
    target=Vector((0,0,.04));cam.location=(0,0,8) if top else (0,-8,1.1)
    cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();d.type='ORTHO';d.ortho_scale=5.3;scene.camera=cam
    for pos,power in [((-3,-4,7),650),((4,3,5),350)]:
        data=bpy.data.lights.new('B09_Softbox','AREA');data.energy=power;data.shape='DISK';data.size=5
        light=bpy.data.objects.new('B09_Softbox',data);rig.objects.link(light);light.location=pos;light.rotation_euler=(target-light.location).to_track_quat('-Z','Y').to_euler()
    scene.world=bpy.data.worlds.get('B09_World') or bpy.data.worlds.new('B09_World');scene.world.use_nodes=True
    bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs['Color'].default_value=(.055,.075,.10,1);bg.inputs['Strength'].default_value=.5
    scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1100;scene.render.resolution_y=850;scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA';scene.render.film_transparent=False
    scene.view_settings.view_transform='Standard';scene.view_settings.look='None'
    path.parent.mkdir(parents=True,exist_ok=True);scene.render.filepath=str(path);bpy.ops.render.render(write_still=True)

def before():
    scene=fresh_scene('B09_Before'); evidence={}
    for name in BOATS:
        objs,record=read_fbx(OUT/'_before_B09'/(name+'.fbx'));evidence[name]=record
        if name==BOATS[0]:
            render(scene,OUT/'_shots'/'B09_before_top.png',True);render(scene,OUT/'_shots'/'B09_before_side.png')
        for ob in objs:bpy.data.objects.remove(ob,do_unlink=True)
    (OUT/'B09_before.json').write_text(json.dumps(evidence,indent=2))
    print('Archived FBXs and before views captured.')

def assets():
    scene=fresh_scene('B09_Build');log=io.StringIO();reports=[]
    for repeat in [1,2]:
        with contextlib.redirect_stdout(log):
            oar=runpy.run_path(str(ROOT/'BlenderScripts'/'SM_LF_Oar.py'))['REPORT']
            boats=runpy.run_path(str(ROOT/'BlenderScripts'/'SM_LF_Rowboat.py'))['REPORTS']
        report={'oar':oar,'meshes':{o.name:signature(o) for o in scene.objects if o.type=='MESH'},'boats':[{k:v for k,v in b.items() if k!='geometry_sha256'} for b in boats]}
        reports.append(report)
    # Bmesh polygon order is immaterial; canonical geometry signatures include all faces/materials.
    assert reports[0]==reports[1],'Repeat run mismatch'
    (OUT/'B09_runs.json').write_text(json.dumps(reports,indent=2));(OUT/'B09_execution.log').write_text(log.getvalue())
    print('Oar + four-variant rowboat script ran twice; canonical reports identical.')

def after():
    baseline=json.loads((OUT/'B09_before.json').read_text());scene=fresh_scene('B09_Audit');audit={}
    for name in BOATS:
        objs,record=read_fbx(OUT/(name+'.fbx'))
        assert json.loads(json.dumps(record['meshes']))==baseline[name]['meshes'],name+' mesh/material/UCX changed'
        expected=[{'name':'SOCKET_Oarlock_'+side,'parent':name,'location_m':[-.12,y,.33],'rotation':[0,0,0],'scale':[1,1,1]} for side,y in [('L',-.672),('R',.672)]]
        assert sorted(record['sockets'],key=lambda x:x['name'])==expected,(name,record['sockets'])
        audit[name]={'render_and_ucx_unchanged':True,'sockets':record['sockets'],'mesh_count':len(record['meshes'])}
        for ob in objs:bpy.data.objects.remove(ob,do_unlink=True)
    objs,oar=read_fbx(OUT/'SM_LF_Oar.fbx');assert len(oar['meshes'])==1
    expected_oar=json.loads((OUT/'B09_runs.json').read_text())[-1]['meshes']['SM_LF_Oar']
    assert list(oar['meshes']['SM_LF_Oar'])==expected_oar,'Oar FBX differs from generated mesh'
    assert oar['sockets']==[{'name':'SOCKET_Grip','parent':'SM_LF_Oar','location_m':[-.70,0,0],'rotation':[0,0,0],'scale':[1,1,1]}]
    assert oar['meshes']['SM_LF_Oar'][1]<300
    for ob in objs:bpy.data.objects.remove(ob,do_unlink=True)
    scene=fresh_scene('B09_Review')
    objects,boat=read_fbx(OUT/'SM_LF_Rowboat.fbx')
    # Hide all build scene objects from review; use reimported exports only.
    source_objs,oar_record=read_fbx(OUT/'SM_LF_Oar.fbx');oar_ob=next(o for o in source_objs if o.type=='MESH')
    for side,sign in [('L',-1),('R',1)]:
        socket=next(o for o in objects if o.type=='EMPTY' and o.name.split('.')[0]=='SOCKET_Oarlock_'+side)
        placed=bpy.data.objects.new('B09_Posed_Oar_'+side,oar_ob.data.copy());scene.collection.objects.link(placed)
        placed.parent=socket;placed.location=(0,0,0);placed.rotation_euler=(math.pi/2,math.radians(16),sign*math.radians(108))
    for ob in source_objs:bpy.data.objects.remove(ob,do_unlink=True)
    render(scene,OUT/'_shots'/'B09_oars_top.png',True);render(scene,OUT/'_shots'/'B09_oars_side.png')
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'B09_Review.blend'))
    result={'blender':bpy.app.version_string,'two_runs_identical':True,'boats':audit,'oar':oar,'pose':'Both collar origins exactly at pin-tip sockets; yaw +/-108 deg, pitch 16 deg, blade roll 90 deg. Staging only.'}
    (OUT/'B09_verified.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
