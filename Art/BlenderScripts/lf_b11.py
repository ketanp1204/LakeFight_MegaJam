"""B11 original ambient life / sky meshes. Blender 5.1, palette only, metres.

Reuses only in-project geometry primitives and common export/palette helpers.
No B08/B09 entrypoint or external asset is used.
"""
import bpy, bmesh, math, json, hashlib
from pathlib import Path
from mathutils import Vector
from mathutils.geometry import tessellate_polygon
import lf_common as lf
from lf_b10 import Geo
ROOT=Path(__file__).resolve().parents[1]; OUT=ROOT/'Export'
NAMES=['SM_LF_Cloud_01','SM_LF_Cloud_02','SM_LF_Cloud_03','SM_LF_Cloud_04',
       'SM_LF_Duck','SM_LF_Swan','SM_LF_Heron',
       'SM_LF_Barrel','SM_LF_LifeRing','SM_LF_Bottle_Message',
       'SM_LF_Bird_Body','SM_LF_Bird_Wing_L','SM_LF_Bird_Wing_R','SM_LF_Bird_Glide']
FLYING=NAMES[-4:]
INSECTS=['SM_LF_Dragonfly','SM_LF_Dragonfly_Wings','SM_LF_Butterfly_Body','SM_LF_Butterfly_Wing_L','SM_LF_Butterfly_Wing_R']
NAMES+=INSECTS
SHOULDERS={'L':(0,-.043,.006),'R':(0,.043,.006)}
BUTTERFLY_HINGES={'L':(.00675,-.00225,0),'R':(.00675,.00225,0)}

def stage():
    scene=bpy.data.scenes.get('B11_Build') or bpy.data.scenes.new('B11_Build')
    bpy.context.window.scene=scene
    scene.unit_settings.system='METRIC'; scene.unit_settings.scale_length=1
    return scene

def tube(g,path,radii,colour,sides=6):
    rings=[]
    for i,p in enumerate(path):
        p=Vector(p)
        tangent=(Vector(path[min(i+1,len(path)-1)])-Vector(path[max(0,i-1)])).normalized()
        # All bird neck paths lie in XZ. Keep a constant lateral axis so an
        # S bend never flips its ring frame through 180 degrees.
        lateral=Vector((0,1,0)); normal=lateral.cross(tangent).normalized()
        rings.append([tuple(p+radii[i]*(normal*math.cos(j*math.tau/sides)+lateral*math.sin(j*math.tau/sides))) for j in range(sides)])
    g.loft(rings,colour)

def torus(g,major,minor,colour,segments=16,sides=6,centre=(0,0,0),axis='Z'):
    verts=[]; faces=[]; mats=[]
    for i in range(segments):
        a=i*math.tau/segments
        for j in range(sides):
            b=j*math.tau/sides
            x=(major+minor*math.cos(b))*math.cos(a)
            y=(major+minor*math.cos(b))*math.sin(a); z=minor*math.sin(b)
            if axis=='X': x,y,z=z,x,y
            verts.append((x+centre[0],y+centre[1],z+centre[2]))
    for i in range(segments):
        for j in range(sides):
            faces.append((i*sides+j,((i+1)%segments)*sides+j,((i+1)%segments)*sides+(j+1)%sides,i*sides+(j+1)%sides))
            mats.append(colour[i] if isinstance(colour,list) else colour)
    g.add(verts,faces,mats)

def cloud(g,variant):
    # Overlapping chunky puffs with a shared flattened base; different skyline per variant.
    recipes=[
        [((0,0,.16),(.38,.22,.18)),((-.28,0,.22),(.22,.20,.25)),
         ((-.03,.01,.34),(.24,.22,.35)),((.27,.015,.19),(.23,.19,.21)),
         ((.05,-.14,.16),(.26,.17,.18))],
        [((0,0,.15),(.41,.24,.17)),((-.25,.01,.34),(.25,.23,.35)),
         ((.02,.04,.40),(.23,.22,.40)),((.29,0,.24),(.23,.20,.26)),
         ((-.20,-.15,.18),(.23,.17,.20)),((.20,-.15,.16),(.25,.17,.18))],
        [((0,0,.12),(.44,.20,.14)),((-.33,0,.17),(.19,.18,.19)),
         ((-.13,.02,.26),(.23,.21,.28)),((.17,0,.22),(.25,.19,.23)),
         ((.37,0,.15),(.18,.16,.17)),((.03,-.14,.13),(.31,.14,.15))],
        [((0,0,.17),(.40,.24,.19)),((-.28,0,.26),(.23,.21,.28)),
         ((-.12,.03,.44),(.23,.22,.45)),((.14,.03,.36),(.25,.23,.37)),
         ((.34,0,.20),(.22,.20,.22)),((-.18,-.15,.17),(.25,.17,.19)),
         ((.18,-.16,.16),(.25,.17,.18))]
    ]
    for c,r in recipes[variant-1]: g.ellipsoid(c,r,'PaintCream',7,4,phase=.14*variant)
    width=[24,36,48,58][variant-1]
    lo=min(v[0] for v in g.v); hi=max(v[0] for v in g.v); scale=width/(hi-lo)
    yc=(min(v[1] for v in g.v)+max(v[1] for v in g.v))/2
    height_scale=[.65,.60,.70,.55][variant-1]
    g.v=[((x-(lo+hi)/2)*scale,(y-yc)*scale,max(0,z)*scale*height_scale) for x,y,z in g.v]

def eyes(g,cx,cy,z,rx=.008):
    for side in [-1,1]: g.ellipsoid((cx,side*cy,z),(rx,rx*.50,rx),'Black',6,1)

def duck(g):
    g.ellipsoid((-.025,0,.010),(.128,.073,.075),'Wood',10,3)
    for side in [-1,1]: g.ellipsoid((-.040,side*.060,.034),(.085,.021,.047),'WoodDark',8,2)
    g.prism([(-.126,.025),(-.185,.059),(-.157,-.008)],.065,'WoodDark')
    tube(g,[(.045,0,.045),(.075,0,.094),(.100,0,.112)],[.034,.030,.028],'Moss',6)
    g.ellipsoid((.103,0,.120),(.048,.043,.046),'Moss',8,2)
    g.prism([(.135,.121),(.204,.111),(.203,.097),(.135,.101)],.049,'PaintOrange')
    eyes(g,.117,.039,.131,.007)
    # Thin pale neck collar keeps a mallard read without reserved PaintGreen.
    tube(g,[(.063,0,.073),(.071,0,.087)],[.033,.032],'PaintCream',6)
    lo=min(v[0] for v in g.v); hi=max(v[0] for v in g.v); s=.35/(hi-lo)
    g.v=[(x*s,y*s,z*s) for x,y,z in g.v]

def swan(body,head):
    body.ellipsoid((-.035,0,.035),(.265,.128,.135),'PaintCream',10,3)
    for side in [-1,1]: body.ellipsoid((-.080,side*.093,.098),(.180,.043,.105),'PaintCream',9,2)
    body.prism([(-.23,.062),(-.385,.138),(-.290,-.008)],.095,'PaintCream')
    tube(head,[(.115,0,.105),(.205,0,.19),(.185,0,.285),(.125,0,.365),(.162,0,.445),(.235,0,.475)],
         [.054,.048,.041,.034,.032,.034],'PaintCream',7)
    head.ellipsoid((.250,0,.480),(.069,.044,.048),'PaintCream',8,2)
    head.prism([(.290,.482),(.375,.462),(.365,.440),(.288,.447)],.034,'PaintOrange')
    head.ellipsoid((.289,0,.474),(.023,.037,.031),'Black',6,2)
    eyes(head,.268,.041,.492,.006)
    allv=body.v+head.v; s=.80/(max(v[0] for v in allv)-min(v[0] for v in allv))
    body.v=[(x*s,y*s,z*s) for x,y,z in body.v]
    head.v=[(x*s,y*s,z*s) for x,y,z in head.v]
    return (.115*s,0,.105*s)

def heron(g):
    g.ellipsoid((-.025,0,.545),(.135,.085,.195),'Metal',8,3)
    for side in [-1,1]: g.ellipsoid((-.055,side*.065,.570),(.106,.025,.15),'Cloth',8,2)
    g.ellipsoid((.050,-.003,.615),(.062,.064,.105),'PaintCream',6,2)
    tube(g,[(.025,0,.662),(.092,0,.735),(.030,0,.805),(.020,0,.875),(.105,0,.939)],
         [.035,.031,.027,.023,.025],'PaintCream',6)
    g.ellipsoid((.134,0,.953),(.062,.039,.043),'Metal',8,2)
    g.prism([(.175,.960),(.325,.939),(.174,.929)],.021,'PaintOrange')
    eyes(g,.150,.036,.967,.006)
    g.beam((0,0,.022),(.010,0,.431),.009,'WoodDark',5,.012)
    for tip in [(.087,-.036,.003),(.087,.036,.003),(-.050,0,.003)]: g.beam((0,0,.020),tip,.005,'WoodDark',4)
    g.beam((.008,.040,.449),(.080,.046,.326),.008,'WoodDark',4)
    g.beam((.080,.046,.326),(-.018,.045,.320),.007,'WoodDark',4)
    for y,dz in [(-.015,0),(.015,0),(0,.012)]: g.beam((.095,y,.973),(.016,y,.970+dz),.005,'Black',4,.001)
    g.prism([(-.10,.465),(-.19,.405),(-.08,.420)],.055,'Metal')
    base=min(v[2] for v in g.v); top=max(v[2] for v in g.v); s=1/(top-base)
    g.v=[(x*s,y*s,(z-base)*s) for x,y,z in g.v]

def barrel(g):
    n=10; specs=[(-.48,.25),(-.40,.29),(0,.32),(.40,.29),(.48,.25)]
    verts=[(x,r*math.cos(j*math.tau/n),r*math.sin(j*math.tau/n)) for x,r in specs for j in range(n)]
    faces=[]; mats=[]
    for i in range(len(specs)-1):
        for j in range(n):
            faces.append((i*n+j,i*n+(j+1)%n,(i+1)*n+(j+1)%n,(i+1)*n+j))
            mats.append('Wood' if j%3 else 'WoodDark')
    faces.extend([tuple(reversed(range(n))),tuple((len(specs)-1)*n+j for j in range(n))]); mats.extend(['Rope','Rope'])
    g.add(verts,faces,mats)
    for x in [-.30,.30]: torus(g,.304,.018,'Metal',10,4,(x,0,0),'X')
    g.beam((-.04,0,.306),(-.04,0,.332),.045,'WoodDark',6)

def lifering(g):
    colours=['PaintCream' if i%4==0 else 'PaintOrange' for i in range(16)]
    torus(g,.34,.080,colours,16,6)
    torus(g,.467,.014,'Rope',12,4)

def bottle(g):
    n=10; specs=[(-.21,.053),(-.185,.077),(.065,.077),(.125,.033),(.220,.030),(.236,.034)]
    rings=[[(x,r*math.cos(j*math.tau/n),r*math.sin(j*math.tau/n)) for j in range(n)] for x,r in specs]
    g.loft(rings,'Cloth')
    g.beam((.232,0,0),(.270,0,0),.026,'Wood',8)
    torus(g,.032,.005,'Rope',8,4,(.215,0,0),'X')
    # A cream parchment panel and solid raised strokes imply the letter through
    # stylized opaque green glass, retaining the shared solid-colour palette.
    outline=[(-.145,-.041),(.020,-.041),(.028,.041),(-.145,.041)]
    panel=Geo(); panel.prism(outline,.004,'PaintCream',-.076)
    g.add(panel.v,panel.f,panel.m)
    for x,z,length in [(-.063,.022,.105),(-.071,.003,.089),(-.088,-.019,.054)]:
        g.box((x,-.079,z),(length,.003,.004),'WoodDark')
    # Roll the visible parchment toward the sky, then tilt the neck upward.
    roll=math.radians(-55)
    g.v=[(x,y*math.cos(roll)-z*math.sin(roll),y*math.sin(roll)+z*math.cos(roll)) for x,y,z in g.v]
    a=math.radians(18)
    g.v=[(x*math.cos(a)-z*math.sin(a),y,x*math.sin(a)+z*math.cos(a)+.016) for x,y,z in g.v]

def flying_body(g,distant=False):
    g.ellipsoid((-.02,0,0),(.14,.060,.046),'PaintCream',6 if distant else 8,1 if distant else 2)
    g.ellipsoid((.132,0,.040),(.045,.039,.038),'PaintCream',6,1)
    g.prism([(.160,.052),(.225,.035),(.160,.023)],.024,'PaintOrange')
    # Horizontal forked tail: two small points behind the body.
    outline=[(-.10,-.025),(-.215,-.060),(-.188,0),(-.215,.060),(-.10,.025)]
    g.loft([[(x,y,z) for x,y in outline] for z in [-.007,.007]],'PaintCream')
    if not distant:
        for side in [-1,1]: g.ellipsoid((.143,side*.035,.051),(.006,.003,.006),'Black',4,1)

def flying_wing(g,side):
    sign=-1 if side=='L' else 1
    rings=[]
    # Diamond airfoil rings: +X leading edge, narrow swept wrist and dark tip.
    for span,cx,front,back,z,thickness in [(0,0,.048,.060,0,.009),
            (.155,-.007,.068,.090,.014,.008),
            (.290,-.035,.058,.078,.010,.005),
            (.407,-.100,.015,.019,-.003,.002)]:
        y=sign*span
        rings.append([(cx+front,y,z),(cx,y,z+thickness),(cx-back,y,z),(cx,y,z-thickness)])
    g.loft(rings,'PaintCream')
    for i in range(8,12): g.m[i]='WoodDark'
    g.m[-1]='WoodDark'

def flying_glide(g):
    flying_body(g,True)
    for side in ['L','R']:
        wing=Geo(); flying_wing(wing,side)
        a=math.radians(-6 if side=='L' else 6); px,py,pz=SHOULDERS[side]
        verts=[(x+px,y*math.cos(a)-z*math.sin(a)+py,y*math.sin(a)+z*math.cos(a)+pz) for x,y,z in wing.v]
        g.add(verts,wing.f,wing.m)

def plane(g,outline,colour):
    # Independent front/back vertices survive FBX duplicate-face cleanup.
    # Micrometre separation avoids adding a special two-sided material.
    n=len(outline)
    verts=[(x,y,z+.000001) for x,y,z in outline]+[(x,y,z-.000001) for x,y,z in outline]
    g.add(verts,[tuple(range(n)),tuple(reversed(range(n,2*n)))],colour)

def dragonfly(g,wings=False):
    if wings:
        for side in [-1,1]:
            for outline,colour in [([(.005,.003),(.022,.060),(-.001,.055),(-.004,.003)],'PaintCream'),
                                    ([(-.006,.004),(-.026,.055),(-.049,.047),(-.014,.003)],'Metal')]:
                plane(g,[(x,side*y,abs(y)*.10) for x,y in outline],colour)
    else:
        g.beam((-.040,0,0),(.014,0,0),.002,'PaintBlue',5,.004)
        g.ellipsoid((.017,0,0),(.012,.006,.006),'Moss',6,1)
        g.ellipsoid((.031,0,.001),(.009,.009,.007),'PaintBlue',6,1)
        for side in [-1,1]:
            g.ellipsoid((.033,side*.007,.004),(.004,.002,.004),'Black',4,1)
            for x in [.009,.017,.024]:
                plane(g,[(x,side*.004,-.002),(x+.008,side*.017,-.008),(x+.002,side*.004,-.002)],'WoodDark')
    g.v=[(x*1.5,y*1.5,z*1.5) for x,y,z in g.v]

def butterfly_body(g):
    g.ellipsoid((-.005,0,0),(.010,.0030,.0030),'WoodDark',6,1)
    g.ellipsoid((.0045,0,0),(.005,.0036,.0036),'WoodDark',6,1)
    g.ellipsoid((.011,0,0),(.003,.0032,.0032),'WoodDark',4,1)
    for side in [-1,1]: plane(g,[(.012,side*.001,.002),(.020,side*.006,.004),(.013,side*.002,.002)],'WoodDark')
    g.v=[(x*1.5,y*1.5,z*1.5) for x,y,z in g.v]

def butterfly_wing(g,side):
    sign=-1 if side=='L' else 1
    outline=[(.012,0),(.035,.025),(.026,.041),(.004,.037),(-.005,.024),(-.018,.035),(-.034,.023),(-.018,0)]
    points=[Vector((x,sign*y,0)) for x,y in outline]; lookup={tuple(v):i for i,v in enumerate(points)}
    triangles=[tuple(tri) if isinstance(tri[0],int) else tuple(lookup[tuple(v)] for v in tri) for tri in tessellate_polygon([points])]
    n=len(points); verts=[(v.x,v.y,.000001) for v in points]+[(v.x,v.y,-.000001) for v in points]
    colours=['PaintCream' if sum(abs(points[i].y) for i in tri)/3>.027 else 'PaintOrange' for tri in triangles]
    g.add(verts,triangles+[tuple(i+n for i in reversed(tri)) for tri in triangles],colours+colours)
    g.v=[(x*1.5,y*1.5,z*1.5) for x,y,z in g.v]

def mesh_object(name,g,col,pivot=(0,0,0),parent=None,cloud_under=False):
    pivot=Vector(pivot)
    me=bpy.data.meshes.new(name); me.from_pydata([tuple(Vector(v)-pivot) for v in g.v],[],g.f); me.update()
    colours=list(dict.fromkeys(g.m))
    if cloud_under: colours.append('Metal')
    allowed={'PaintBlue'} if name=='SM_LF_Dragonfly' else set()
    assert not (set(colours)&lf.TEAM_COLOURS)-allowed
    for c in colours: me.materials.append(lf.mat(c))
    for p,c in zip(me.polygons,g.m): p.material_index=colours.index(c)
    bm=bmesh.new(); bm.from_mesh(me); bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces)); bm.to_mesh(me); bm.free()
    if cloud_under:
        for p in me.polygons:
            if p.normal.z<-.65: p.material_index=colours.index('Metal')
    ob=bpy.data.objects.new(name,me); col.objects.link(ob); ob.parent=parent; ob.location=pivot
    lf.flat_shade(ob); return ob

def signature(ob):
    me=ob.data; me.calc_loop_triangles()
    payload={'vertices':[[round(c,7) for c in v.co] for v in me.vertices],
             'faces':[[*p.vertices,p.material_index] for p in me.polygons],
             'materials':[m.name for m in me.materials]}
    bm=bmesh.new(); bm.from_mesh(me)
    result={'name':ob.name,'triangles':len(me.loop_triangles),
            'origin_local_m':[round(c,7) for c in ob.location],
            'parent':ob.parent.name if ob.parent else None,
            'scale':list(ob.scale),'materials':payload['materials'],
            'closed_components':all(e.is_manifold for e in bm.edges),
            'degenerate_faces':sum(f.calc_area()<1e-12 for f in bm.faces),
            'flat_shaded':all(not p.use_smooth for p in me.polygons),
            'geometry_sha256':hashlib.sha256(json.dumps(payload,sort_keys=True).encode()).hexdigest()}
    bm.free(); return result

def build(name):
    stage(); col=lf.fresh_collection(name); g=Geo(); head=None; pivot=None
    if 'Cloud_' in name: cloud(g,int(name[-2:]))
    elif name=='SM_LF_Duck': duck(g)
    elif name=='SM_LF_Swan':
        head=Geo(); pivot=swan(g,head)
    elif name=='SM_LF_Heron': heron(g)
    elif name=='SM_LF_Barrel': barrel(g)
    elif name=='SM_LF_LifeRing': lifering(g)
    elif name=='SM_LF_Bottle_Message': bottle(g)
    elif name=='SM_LF_Bird_Body': flying_body(g)
    elif name in ['SM_LF_Bird_Wing_L','SM_LF_Bird_Wing_R']: flying_wing(g,name[-1])
    elif name=='SM_LF_Bird_Glide': flying_glide(g)
    elif name=='SM_LF_Dragonfly': dragonfly(g)
    elif name=='SM_LF_Dragonfly_Wings': dragonfly(g,True)
    elif name=='SM_LF_Butterfly_Body': butterfly_body(g)
    elif name in ['SM_LF_Butterfly_Wing_L','SM_LF_Butterfly_Wing_R']: butterfly_wing(g,name[-1])
    else: raise ValueError(name)
    root=mesh_object(name,g,col,cloud_under='Cloud_' in name)
    if head: mesh_object(name+'_HeadNeck',head,col,pivot,root)
    bpy.context.view_layer.update()
    parts=[signature(o) for o in sorted(col.objects,key=lambda o:o.name)]
    coords=[o.matrix_world@v.co for o in col.objects for v in o.data.vertices]
    lo=[min(v[k] for v in coords) for k in range(3)]; hi=[max(v[k] for v in coords) for k in range(3)]
    budget=100 if name in INSECTS else 150 if name in FLYING else 300 if name in ['SM_LF_Barrel','SM_LF_LifeRing','SM_LF_Bottle_Message'] else 400
    report={'asset':name,'triangles':sum(p['triangles'] for p in parts),'budget_exclusive':budget,
            'dimensions_m':[round(hi[k]-lo[k],6) for k in range(3)],
            'z_range_m':[round(lo[2],6),round(hi[2],6)],
            'root_origin_m':list(root.location),'parts':parts}
    if name.startswith('SM_LF_Bird_Wing_'):
        report['assembly_shoulder_m']=list(SHOULDERS[name[-1]])
        report['flap_axis']='local X; left up = negative angle, right up = positive angle'
    if name in INSECTS:
        report['authored_enlargement']=1.5
        report['thin_planes']=name in ['SM_LF_Dragonfly_Wings','SM_LF_Butterfly_Wing_L','SM_LF_Butterfly_Wing_R']
    if name=='SM_LF_Dragonfly_Wings': report['assembly_hinge_m']=[.0255,0,0]
    if name.startswith('SM_LF_Butterfly_Wing_'): report['assembly_hinge_m']=list(BUTTERFLY_HINGES[name[-1]])
    assert report['triangles']<budget,report
    assert all((p['closed_components'] or name in INSECTS) and p['flat_shaded'] and p['degenerate_faces']==0 for p in parts),report
    if name in ['SM_LF_Duck','SM_LF_Swan','SM_LF_Barrel','SM_LF_LifeRing','SM_LF_Bottle_Message']:
        assert lo[2]<0<hi[2],report
    assert len(parts)==(2 if name=='SM_LF_Swan' else 1),report
    lf.export_fbx(col,name)
    (OUT/'_reports').mkdir(exist_ok=True)
    (OUT/'_reports'/(name+'_B11.json')).write_text(json.dumps(report,indent=2))
    return report
