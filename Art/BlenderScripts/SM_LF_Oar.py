"""B09 original 2.1 m wooden oar; collar pivot at zero, blade +X, metres."""
import sys, importlib, math, json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import bpy
import lf_common as lf
importlib.reload(lf)

NAME='SM_LF_Oar'
col=lf.fresh_collection(NAME)
vertices=[]; faces=[]; slots=[]
def profile(stations,material):
    start=len(vertices); n=8
    for x,y,z in stations:
        vertices.extend((x,y*math.cos(i*math.tau/n),z*math.sin(i*math.tau/n)) for i in range(n))
    for ring in range(len(stations)-1):
        for i in range(n):
            a=start+ring*n+i; b=start+ring*n+(i+1)%n
            faces.append((a,b,b+n,a+n));slots.append(material)
    faces.extend([tuple(reversed(range(start,start+n))),tuple(range(start+(len(stations)-1)*n,start+len(stations)*n))]);slots.extend([material]*2)

# Round grip, slender round loom, tapered shoulder, narrow elliptical blade.
# Blade region x=.95..1.40 is 45 cm long, maximum width 12 cm.
profile([(-.70,.015,.015),(-.68,.023,.023),(-.53,.023,.023),(-.50,.019,.019),
         (.78,.019,.019),(.95,.035,.012),(1.04,.057,.010),
         (1.30,.060,.009),(1.37,.042,.008),(1.40,.012,.005)],0)
# Closed leather sleeve around the oarlock pivot, subtle raised end seams.
profile([(-.075,.026,.026),(-.060,.029,.029),(.060,.029,.029),(.075,.026,.026)],1)
me=bpy.data.meshes.new(NAME);me.from_pydata(vertices,[],faces);me.update()
ob=bpy.data.objects.new(NAME,me);col.objects.link(ob)
me.materials.append(lf.mat('Wood'))
leather=bpy.data.materials.get('M_LF_Leather') or bpy.data.materials.new('M_LF_Leather')
leather.use_nodes=True; shader=next(n for n in leather.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Base Color'].default_value=(.20,.09,.035,1);shader.inputs['Roughness'].default_value=.9
leather.diffuse_color=(.20,.09,.035,1);me.materials.append(leather)
for p,slot in zip(me.polygons,slots):p.material_index=slot;p.use_smooth=False
grip=bpy.data.objects.new('SOCKET_Grip',None);col.objects.link(grip);grip.parent=ob
grip.location=(-.70,0,0);grip.empty_display_type='ARROWS';grip.empty_display_size=.08
bpy.context.view_layer.update();me.calc_loop_triangles()
assert len(me.loop_triangles)<300
assert abs(ob.dimensions.x-2.1)<1e-6
assert ob.location.length==0 and all(x==1 for x in ob.scale)
lf.export_fbx(col,NAME)
REPORT={'asset':NAME,'triangles':len(me.loop_triangles),'dimensions_m':list(ob.dimensions),'origin':'collar centre (0,0,0)','blade_direction':'+X','blade_length_m':.45,'blade_width_m':.12,'socket':{'name':grip.name,'parent':ob.name,'location_m':list(grip.location),'rotation':list(grip.rotation_euler),'scale':list(grip.scale)},'ucx':False}
print(json.dumps(REPORT))
