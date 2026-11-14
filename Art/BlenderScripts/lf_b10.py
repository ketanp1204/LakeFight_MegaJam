"""B10 original low-poly environment and two icon revisions. Blender 5.1.

Only lf_common is imported: B08/B09 entrypoints are never executed here.
Metres, base-centre origins, flat palette materials, deterministic topology.
"""
import bpy, bmesh, math, json, hashlib
from pathlib import Path
from mathutils import Vector
import lf_common as lf

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Export'
MESHES = ['SM_LF_GroundPatch_01','SM_LF_GroundPatch_02',
          'SM_LF_Hill_Far_01','SM_LF_Hill_Far_02','SM_LF_Hill_Far_03',
          'SM_LF_Bush_01','SM_LF_Bush_02','SM_LF_Log','SM_LF_Fence_Section',
          'SM_LF_TreeCluster_01']
ICONS = ['ICON_LF_Paddle','ICON_LF_FishGold']

def stage():
    scene = bpy.data.scenes.get('B10_Build') or bpy.data.scenes.new('B10_Build')
    bpy.context.window.scene = scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1
    return scene

class Geo:
    def __init__(self): self.v=[]; self.f=[]; self.m=[]
    def add(self, verts, faces, colour):
        n=len(self.v); self.v.extend(verts)
        self.f.extend(tuple(n+i for i in f) for f in faces)
        self.m.extend([colour]*len(faces) if isinstance(colour,str) else colour)
    def loft(self,rings,colour,caps=True):
        n=len(rings[0]); v=[p for ring in rings for p in ring]; f=[]
        for k in range(len(rings)-1):
            for j in range(n): f.append((k*n+j,k*n+(j+1)%n,(k+1)*n+(j+1)%n,(k+1)*n+j))
        if caps: f += [tuple(reversed(range(n))),tuple((len(rings)-1)*n+j for j in range(n))]
        self.add(v,f,colour)
    def beam(self,a,b,r,colour,sides=7,r2=None):
        a,b=Vector(a),Vector(b); q=(b-a).to_track_quat('Z','Y'); r2=r if r2 is None else r2
        rings=[[tuple(c+q@Vector((radius*math.cos(j*math.tau/sides),radius*math.sin(j*math.tau/sides),0)))
                for j in range(sides)] for c,radius in [(a,r),(b,r2)]]
        self.loft(rings,colour)
    def box(self,c,s,colour,tilt=0):
        x,y,z=c; a,b,d=[i/2 for i in s]
        self.add([(x+u*math.cos(tilt)-w*math.sin(tilt),y+v,z+u*math.sin(tilt)+w*math.cos(tilt))
                  for u,v,w in [(-a,-b,-d),(a,-b,-d),(a,b,-d),(-a,b,-d),(-a,-b,d),(a,-b,d),(a,b,d),(-a,b,d)]],
                 [(3,2,1,0),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],colour)
    def ellipsoid(self,c,r,colour,segments=10,rings=3,phase=0):
        x,y,z=c; rx,ry,rz=r; v=[(x,y,z-rz)]
        for k in range(1,rings+1):
            p=-math.pi/2+k*math.pi/(rings+1)
            for j in range(segments):
                a=j*math.tau/segments+phase
                v.append((x+rx*math.cos(p)*math.cos(a),y+ry*math.cos(p)*math.sin(a),z+rz*math.sin(p)))
        v.append((x,y,z+rz)); top=len(v)-1; f=[]
        for j in range(segments): f.append((0,1+(j+1)%segments,1+j))
        for k in range(rings-1):
            for j in range(segments):
                a=1+k*segments+j; b=1+k*segments+(j+1)%segments
                f.append((a,b,b+segments,a+segments))
        for j in range(segments): f.append((1+(rings-1)*segments+j,1+(rings-1)*segments+(j+1)%segments,top))
        self.add(v,f,colour)
    def prism(self,outline,depth,colour,y=0):
        n=len(outline)
        self.add([(x,y+d,z) for d in [-depth/2,depth/2] for x,z in outline],
                 [tuple(reversed(range(n))),tuple(range(n,2*n))]+[(j,(j+1)%n,(j+1)%n+n,j+n) for j in range(n)],colour)

def ground(g,variant):
    n=13; verts=[]
    bumps=([(-8,-5,1.1,8),(8,7,1.35,9),(-9,11,.7,6)] if variant==1
           else [(-10,7,1.3,9),(6,-8,1.15,8),(10,11,.9,7)])
    for j in range(n):
        for i in range(n):
            x=-20+i*40/(n-1); y=-20+j*40/(n-1)
            edge=i in [0,n-1] or j in [0,n-1]
            if not edge:
                x+=.5*math.sin(i*2.1+j*1.7+variant)
                y+=.5*math.cos(i*1.9-j*2.3+variant)
            envelope=(max(0,1-(x/20)**2)*max(0,1-(y/20)**2))**.75
            h=sum(a*math.exp(-((x-bx)**2+(y-by)**2)/(w*w)) for bx,by,a,w in bumps)
            z=0 if edge else min(1.45,h*envelope)
            verts.append((x,y,z))
    faces=[]; mats=[]
    for j in range(n-1):
        for i in range(n-1):
            a=j*n+i; b=a+1; c=a+n; d=c+1
            faces.extend([(a,b,d),(a,d,c)] if (i+j+variant)%2 else [(a,b,c),(b,d,c)])
            # Broad grass patches, not noisy per-triangle confetti.
            colour='Reed' if (i-3)**2+(j-7)**2<13 else 'Moss'
            mats.extend([colour,colour])
    g.add(verts,faces,mats)

def hill(g,variant):
    width,depth,height=[(130,105,24),(170,120,33),(195,145,39)][variant-1]
    n=16; rings=[]
    for k,(radius,z) in enumerate([(1,0),(.72,.38),(.34,.82)]):
        ring=[]
        for j in range(n):
            a=j*math.tau/n
            wobble=1+.065*math.sin(3*a+variant)*math.sin(a)**2
            ring.append((width/2*radius*wobble*math.cos(a)+k*width*.018,
                         depth/2*radius*wobble*math.sin(a)-k*depth*.025,
                         height*z*(1+.10*math.sin(2*a+variant))))
        rings.append(ring)
    # Cloth is the shared palette's cooler desaturated green, no team paints.
    g.loft(rings,'Cloth',caps=False)
    g.add(rings[-1]+[(width*.035,-depth*.03,height)],[(j,(j+1)%n,n) for j in range(n)],'Cloth')
    g.add(rings[0],[tuple(reversed(range(n)))],'Cloth')

def bush(g,variant):
    if variant==1:
        g.ellipsoid((-.22,0,.49),(.48,.48,.49),'Moss',9,3)
        g.ellipsoid((.29,.04,.58),(.48,.43,.53),'Reed',9,3,phase=.25)
        g.ellipsoid((.04,.20,.82),(.39,.39,.43),'Moss',8,2)
    else:
        g.ellipsoid((0,0,.65),(.68,.57,.65),'Moss',10,4,phase=.15)
        g.ellipsoid((-.43,-.04,.43),(.32,.36,.39),'Reed',8,2)
        g.ellipsoid((.42,.02,.51),(.34,.40,.41),'Moss',8,2)

def log(g):
    # Fallen tapered 2.6m trunk, exposed end-grain disks and a snapped branch.
    g.beam((-1.3,0,.24),(1.3,.02,.22),.24,'WoodDark',9,.21)
    g.beam((-1.305,0,.24),(-1.32,0,.24),.195,'Rope',9)
    g.beam((1.305,.02,.22),(1.32,.02,.22),.17,'Wood',9)
    g.beam((.12,0,.30),(.40,.24,.69),.085,'WoodDark',6,.055)
    g.beam((.40,.24,.69),(.408,.247,.705),.045,'Rope',6)

def fence(g):
    for x,h,lean in [(-.91,1.13,-.045),(.91,1.03,.055)]:
        g.box((x,0,h/2),(.18,.19,h),'WoodDark',lean)
        g.prism([(x-.09,h-.04),(x,h+.12),(x+.09,h-.04)],.19,'Wood',0)
    for z,tilt in [(.40,-.025),(.79,.035)]: g.box((0,-.04,z),(2,.10,.15),'Wood',tilt)
    g.box((0,.035,.59),(1.77,.08,.095),'WoodDark',.35)
    for x in [-.90,.90]:
        for z in [.40,.79]: g.beam((x,-.102,z),(x,-.112,z),.022,'Metal',5)

def cluster(g):
    trees=[(-5.4,-1.8,5.8,'pine'),(-2.4,3.0,6.8,'round'),(1,3.8,7.4,'pine'),
           (5.1,1.5,6.2,'round'),(3.4,-3.0,5.7,'pine'),(-1.4,-3,5.5,'round'),
           (-.2,.4,6.7,'round')]
    for t,(x,y,h,kind) in enumerate(trees):
        g.beam((x,y,0),(x+.12,y,h*.73),.21,'WoodDark',7,.10)
        if kind=='pine':
            for layer,(z,r,dh) in enumerate([(.20,.31,.46),(.40,.25,.43),(.61,.18,.39)]):
                n=8; radius=h*r
                base=[(x+radius*math.cos(j*math.tau/n+t*.35),y+radius*math.sin(j*math.tau/n+t*.35),h*z) for j in range(n)]
                g.add(base+[(x+.12,y-.06,h*(z+dh))],
                      [tuple(reversed(range(n)))]+[(j,(j+1)%n,n) for j in range(n)],'Moss' if layer%2==0 else 'Cloth')
        else:
            for j,(dx,dy,dz,r) in enumerate([(-.65,0,-.3,1.55),(.65,.12,0,1.65),(.04,-.35,.95,1.4)]):
                g.ellipsoid((x+dx,y+dy,h*.66+dz),(r,r*.88,r*1.04),'Moss' if j!=1 else 'Reed',9,3,phase=t*.3)

def paddle(g):
    g.beam((-1.10,0,0),(.29,0,0),.038,'Rope',10)
    g.beam((-1.10,0,-.15),(-1.10,0,.15),.052,'Rope',10)
    g.prism([(.65+.59*math.cos(j*math.tau/20),.23*math.sin(j*math.tau/20)) for j in range(20)],.065,'Rope')

def fishgold(g):
    # Bonus-fish palette exception already recorded in lf_common.
    g.ellipsoid((.10,0,0),(.70,.18,.33),'PaintYellow',12,5)
    g.prism([(-.44,0),(-.94,.34),(-.86,0),(-.94,-.34)],.12,'PaintOrange')
    g.prism([(-.17,.26),(.08,.57),(.28,.27)],.07,'PaintOrange')
    g.prism([(-.1,-.25),(.22,-.47),(.35,-.24)],.07,'PaintOrange')
    g.ellipsoid((.54,-.164,.12),(.095,.035,.095),'PaintCream',10,3)
    g.ellipsoid((.56,-.198,.12),(.051,.018,.051),'Black',10,3)
    # Thin ivory-white arc sitting on the upper visible rim of the body.
    for a,b in [(1.05,1.35),(1.35,1.65),(1.65,1.95),(1.95,2.25)]:
        g.beam((.10+.66*math.cos(a),-.104,.31*math.sin(a)),
               (.10+.66*math.cos(b),-.104,.31*math.sin(b)),.018,'PaintCream',6)
    for x,z,r in [(-.68,.65,.115),(.69,.63,.095),(.98,-.24,.075)]:
        g.prism([(x+math.cos(j*math.pi/4)*(r if j%2==0 else r*.26),
                  z+math.sin(j*math.pi/4)*(r if j%2==0 else r*.26)) for j in range(8)],.025,'PaintCream',-.13)

def inspect(ob):
    me=ob.data; me.calc_loop_triangles()
    payload={'vertices':[[round(c,6) for c in v.co] for v in me.vertices],
             'faces':[[*p.vertices,p.material_index] for p in me.polygons],
             'materials':[m.name for m in me.materials]}
    edges={}
    for p in me.polygons:
        ids=list(p.vertices)
        for a,b in zip(ids,ids[1:]+ids[:1]):
            e=tuple(sorted((a,b))); edges[e]=edges.get(e,0)+1
    boundary=[e for e,n in edges.items() if n==1]
    return {'name':ob.name,'triangles':len(me.loop_triangles),
            'dimensions_m':[round(c,5) for c in ob.dimensions],
            'origin_m':list(ob.location),'scale':list(ob.scale),
            'z_range_m':[round(min(v.co.z for v in me.vertices),6),round(max(v.co.z for v in me.vertices),6)],
            'materials':payload['materials'],'boundary_edges':len(boundary),
            'boundary_z_max_abs_m':max([abs(me.vertices[i].co.z) for e in boundary for i in e],default=0),
            'flat_shaded':all(not p.use_smooth for p in me.polygons),
            'geometry_sha256':hashlib.sha256(json.dumps(payload,sort_keys=True).encode()).hexdigest()}

def build(name):
    stage(); col=lf.fresh_collection(name); g=Geo()
    if 'GroundPatch' in name: ground(g,int(name[-2:]))
    elif 'Hill_Far' in name: hill(g,int(name[-2:]))
    elif 'Bush' in name: bush(g,int(name[-2:]))
    elif name=='SM_LF_Log': log(g)
    elif name=='SM_LF_Fence_Section': fence(g)
    elif name=='SM_LF_TreeCluster_01': cluster(g)
    elif name=='ICON_LF_Paddle': paddle(g)
    elif name=='ICON_LF_FishGold': fishgold(g)
    else: raise ValueError(name)
    if name.startswith('SM_') and not ('GroundPatch' in name or 'Hill_Far' in name):
        base=min(v[2] for v in g.v)
        g.v=[(x,y,z-base) for x,y,z in g.v]
    me=bpy.data.meshes.new(name); me.from_pydata(g.v,[],g.f); me.update()
    colours=list(dict.fromkeys(g.m))
    for colour in colours: me.materials.append(lf.mat(colour))
    for p,colour in zip(me.polygons,g.m): p.material_index=colours.index(colour)
    bm=bmesh.new(); bm.from_mesh(me)
    if 'Hill_Far' in name: bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-5)
    # Ground tiles are open heightfields, not closed volumes. Their authored
    # winding is +Z; recalc_face_normals can choose the underside as outside.
    if 'GroundPatch' not in name:
        bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    bm.to_mesh(me); bm.free()
    if 'GroundPatch' in name:
        assert all(p.normal.z > 0 for p in me.polygons), 'Ground top faces must point +Z'
    ob=bpy.data.objects.new(name,me); col.objects.link(ob); lf.flat_shade(ob)
    bpy.context.view_layer.update(); report=inspect(ob)
    limit=800 if 'GroundPatch' in name else 3000 if 'Cluster' in name else 200 if name in ['SM_LF_Log','SM_LF_Fence_Section'] else 1000 if name.startswith('ICON') else 300
    assert report['triangles']<limit,report
    if name.startswith('SM_'):
        assert not set(colours)&lf.TEAM_COLOURS
        assert report['z_range_m'][0]>=-1e-6,report
    if 'GroundPatch' in name:
        assert report['boundary_z_max_abs_m']==0 and report['z_range_m'][1]<=1.5
    else: assert report['boundary_edges']==0,report
    lf.export_fbx(col,name)
    report['budget_exclusive']=limit
    (OUT/'_reports').mkdir(exist_ok=True)
    (OUT/'_reports'/(name+'_B10.json')).write_text(json.dumps(report,indent=2))
    return report
