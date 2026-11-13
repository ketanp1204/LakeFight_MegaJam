"""Original B08 geometry and fixed orthographic icon stage; paths relative to this file.
Run the named asset entrypoints through Blender MCP. No external assets.
"""
import bpy, math, json, hashlib, struct
from pathlib import Path
from mathutils import Vector
import lf_common as lf

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Export'
UI = OUT / 'UI'
SCENE = 'B08_Review'

def link(ob,col):
    ob['b08_order']=len(col.objects)
    return lf.link(ob,col)

def stage():
    scene = bpy.data.scenes.get(SCENE) or bpy.data.scenes.new(SCENE)
    bpy.context.window.scene = scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1
    return scene

def primitive(col, kind, pos, scale, colour, rotation=None, **kw):
    if kind == 'ico':
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=1, location=pos)
    elif kind == 'uv':
        bpy.ops.mesh.primitive_uv_sphere_add(segments=12, ring_count=6, radius=1, location=pos)
    elif kind == 'cyl':
        bpy.ops.mesh.primitive_cylinder_add(vertices=12, radius=1, depth=2, location=pos, **kw)
    else:
        bpy.ops.mesh.primitive_cube_add(size=2, location=pos)
    ob = link(bpy.context.object, col)
    ob.scale = scale
    if rotation: ob.rotation_euler = rotation
    ob.data.materials.append(lf.mat(colour))
    lf.flat_shade(ob)
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    return ob

def beam(col, a, b, radius, colour, sides=6):
    a,b=Vector(a),Vector(b)
    bpy.ops.mesh.primitive_cylinder_add(vertices=sides, radius=radius, depth=(b-a).length, location=(a+b)/2)
    ob=link(bpy.context.object,col)
    ob.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler()
    ob.data.materials.append(lf.mat(colour))
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    return ob

def prism(col, coords, depth, colour):
    n=len(coords)
    verts=[(x,y,z) for y in [-depth/2,depth/2] for x,z in coords]
    faces=[tuple(reversed(range(n))),tuple(range(n,2*n))]
    faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    me=bpy.data.meshes.new('B08_Polygon'); me.from_pydata(verts,[],faces); me.update()
    ob=bpy.data.objects.new('B08_Polygon',me); link(ob,col); me.materials.append(lf.mat(colour))
    return ob

def fish(col, gold):
    body='PaintYellow' if gold else 'Reed'
    fin='PaintOrange' if gold else 'WoodDark'
    primitive(col,'uv',(.12,0,0),(.70,.18,.33),body)
    prism(col,[(-.44,0),(-.94,.34),(-.86,0),(-.94,-.34)],.12,fin)
    prism(col,[(-.17,.26),(.08,.57),(.28,.27)],.07,fin)
    prism(col,[(-.1,-.25),(.22,-.47),(.35,-.24)],.07,fin)
    primitive(col,'uv',(.54,-.164,.12),(.095,.035,.095),'PaintCream')
    primitive(col,'uv',(.56,-.198,.12),(.051,.018,.051),'Black')

def crew(col, team):
    paint='Paint'+team
    primitive(col,'uv',(0,.04,-.42),(.75,.24,.39),paint)
    primitive(col,'uv',(0,0,.26),(.37,.25,.43),'PaintCream')
    # Split front vest panels, dark shirt/zip and cream collar.
    primitive(col,'box',(0,-.203,-.43),(.07,.025,.27),'WoodDark')
    for x in [-.36,.36]:
        primitive(col,'box',(x,-.22,-.43),(.19,.045,.24),paint)
        primitive(col,'box',(x,-.273,-.41),(.16,.012,.025),'Rope')
    for x in [-.125,.125]:
        primitive(col,'ico',(x,-.247,.32),(.036,.025,.039),'Black')
    primitive(col,'ico',(0,-.278,.18),(.075,.07,.07),'PaintCream')
    # Team band on the captain cap keeps Blue readable with its cream crown.
    if team=='Blue':
        primitive(col,'cyl',(0,0,.69),(.40,.30,.16),'PaintCream')
        primitive(col,'cyl',(0,0,.57),(.405,.303,.045),paint)
        primitive(col,'uv',(0,-.27,.54),(.43,.29,.05),'Black')
        primitive(col,'ico',(0,-.301,.71),(.065,.025,.073),'PaintYellow')
        prism(col,[(-.31,.13),(-.24,-.08),(0,-.26),(.24,-.08),(.31,.13),(.12,.02),(-.12,.02)],.09,'WoodDark').location.y=-.25
    else:
        # Sloped bucket crown with broad thin brim.
        bpy.ops.mesh.primitive_cone_add(vertices=12,radius1=.43,radius2=.31,depth=.28,location=(0,0,.69))
        ob=link(bpy.context.object,col); ob.data.materials.append(lf.mat(paint))
        primitive(col,'cyl',(0,0,.55),(.53,.36,.045),paint)
        if team=='Green':
            for sign in [-1,1]:
                ob=primitive(col,'ico',(sign*.13,-.275,.095),(.17,.052,.065),'WoodDark')
                ob.rotation_euler.y=sign*-.25

def build(name):
    stage(); col=lf.fresh_collection(name)
    if name=='SM_LF_FishSpot':
        for i in range(10):
            a=i*math.tau/10; r=1.03
            primitive(col,'ico',(r*math.cos(a),r*math.sin(a),.015+.008*(i%3)),(.22+.018*(i%2),.16,.044+.006*(i%3)),'PaintCream')
    elif name=='SM_LF_BankZone':
        corners=[(-2.38,-1.38,.045),(2.38,-1.38,.045),(2.38,1.38,.045),(-2.38,1.38,.045)]
        for i,a in enumerate(corners): beam(col,a,corners[(i+1)%4],.035,'Rope')
        for x,y in [(-2.38,-1.38),(0,-1.38),(2.38,-1.38),(-2.38,1.38),(0,1.38),(2.38,1.38),(-2.38,0),(2.38,0)]:
            primitive(col,'ico',(x,y,.075),(.12,.12,.075),'PaintOrange')
    elif name.startswith('ICON_LF_Crew_'): crew(col,name.rsplit('_',1)[1])
    elif name in ['ICON_LF_Fish','ICON_LF_FishGold']: fish(col,name.endswith('Gold'))
    elif name=='ICON_LF_Clock':
        rot=(math.pi/2,0,0)
        primitive(col,'cyl',(0,0,0),(.77,.77,.10),'WoodDark',rot)
        primitive(col,'cyl',(0,-.11,0),(.66,.66,.025),'PaintCream',rot)
        for i in range(12):
            a=i*math.tau/12
            ob=primitive(col,'box',(.55*math.sin(a),-.15,.55*math.cos(a)),(.025,.012,.065),'WoodDark')
            ob.rotation_euler.y=a
        beam(col,(0,-.19,0),(0,-.19,.40),.035,'Black')
        beam(col,(0,-.19,0),(.31,-.19,-.18),.035,'Black')
        primitive(col,'uv',(0,-.20,0),(.066,.022,.066),'PaintOrange')
        primitive(col,'box',(0,0,.84),(.17,.08,.055),'PaintOrange')
    elif name=='ICON_LF_Paddle':
        beam(col,(-.5,0,-.71),(.48,0,.75),.048,'Wood')
        beam(col,(.32,0,.85),(.65,0,.63),.06,'WoodDark')
        prism(col,[(-.70,-.85),(-.28,-.94),(-.11,-.61),(-.21,-.28),(-.45,-.25),(-.75,-.56)],.10,'Wood')
    else: raise ValueError(name)
    # Explicit merge avoids Blender's name-dependent object-join ordering on reruns.
    bpy.context.view_layer.update()
    verts=[]; faces=[]; slots=[]; face_slots=[]
    for part in sorted(col.objects,key=lambda o:o['b08_order']):
        offset=len(verts); verts.extend(tuple(part.matrix_world@v.co) for v in part.data.vertices)
        for mat in part.data.materials:
            if mat not in slots: slots.append(mat)
        for poly in part.data.polygons:
            faces.append(tuple(offset+i for i in poly.vertices)); face_slots.append(slots.index(part.data.materials[poly.material_index]))
    for part in list(col.objects): bpy.data.objects.remove(part,do_unlink=True)
    ordered=sorted(zip(faces,face_slots),key=lambda pair:(pair[0],pair[1]))
    faces=[pair[0] for pair in ordered]; face_slots=[pair[1] for pair in ordered]
    me=bpy.data.meshes.new(name); me.from_pydata(verts,[],faces); me.update()
    ob=bpy.data.objects.new(name,me); col.objects.link(ob)
    for mat in slots: me.materials.append(mat)
    for poly,slot in zip(me.polygons,face_slots): poly.material_index=slot
    lf.export_fbx(col,name)
    report=inspect(ob)
    limit=400 if name.endswith('FishSpot') else 600 if name.endswith('BankZone') else 1000
    assert report['triangles']<limit,report
    assert ob.scale[:]==(1,1,1) and not ob.modifiers
    if name.startswith('ICON_'): render(name,Vector((0,0,0)),(0,-6,0),2.25,UI/(name+'.png'))
    else: render(name,Vector((0,0,0)),(4,-6,7),6.5 if name.endswith('BankZone') else 3.4,OUT/'_shots'/(name+'_B08.png'),640)
    report['image']=str(UI/(name+'.png')) if name.startswith('ICON_') else str(OUT/'_shots'/(name+'_B08.png'))
    report['png_sha256']=hashlib.sha256(Path(report['image']).read_bytes()).hexdigest()
    return report

def inspect(ob):
    me=ob.data; me.calc_loop_triangles()
    payload={'vertices':[[round(v,7) for v in x.co] for x in me.vertices], 'faces':[[*p.vertices,p.material_index] for p in me.polygons], 'materials':[m.name for m in me.materials]}
    coords=[ob.matrix_world@v.co for v in me.vertices]
    return {'name':ob.name,'triangles':len(me.loop_triangles),'dimensions_m':[round(x,5) for x in ob.dimensions], 'origin':list(ob.location),'z_range':[round(min(v.z for v in coords),5),round(max(v.z for v in coords),5)],'materials':payload['materials'],'geometry_sha256':hashlib.sha256(json.dumps(payload,sort_keys=True).encode()).hexdigest(),'objects':1,'ucx':False}

def render(name,target,offset,scale,path,size=256):
    scene=stage(); rig=lf.fresh_collection('B08_RenderRig')
    for col in scene.collection.children:
        col.hide_render=col.name not in [name,rig.name]
    camdata=bpy.data.cameras.new('B08_Ortho'); cam=bpy.data.objects.new('B08_Ortho',camdata); rig.objects.link(cam)
    cam.location=target+Vector(offset); cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler(); camdata.type='ORTHO'; camdata.ortho_scale=scale; scene.camera=cam
    for loc,power,area in [((-3,-4,6),450,5),((4,-2,2),180,4)]:
        data=bpy.data.lights.new('B08_Softbox','AREA'); data.energy=power; data.shape='DISK'; data.size=area
        light=bpy.data.objects.new('B08_Softbox',data); rig.objects.link(light); light.location=loc; light.rotation_euler=(target-light.location).to_track_quat('-Z','Y').to_euler()
    scene.world=bpy.data.worlds.get('B08_World') or bpy.data.worlds.new('B08_World'); scene.world.use_nodes=True
    bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND'); bg.inputs['Color'].default_value=(.65,.72,.85,1); bg.inputs['Strength'].default_value=.35
    scene.render.engine='BLENDER_EEVEE'; scene.render.resolution_x=size; scene.render.resolution_y=size; scene.render.resolution_percentage=100
    scene.render.film_transparent=True; scene.render.image_settings.file_format='PNG'; scene.render.image_settings.color_mode='RGBA'; scene.render.image_settings.color_depth='8'
    scene.view_settings.view_transform='Standard'; scene.view_settings.look='None'; scene.view_settings.exposure=0; scene.view_settings.gamma=1
    path.parent.mkdir(parents=True,exist_ok=True); scene.render.filepath=str(path); bpy.ops.render.render(write_still=True)
    # Blender embeds date/render duration in PNG text chunks even with stamps off.
    # Remove metadata only, preserving the exact compressed pixels and alpha.
    data=path.read_bytes(); clean=data[:8]; offset=8
    while offset<len(data):
        length=struct.unpack('>I',data[offset:offset+4])[0]; end=offset+length+12
        if data[offset+4:offset+8]!=b'tEXt': clean+=data[offset:end]
        offset=end
    path.write_bytes(clean)
