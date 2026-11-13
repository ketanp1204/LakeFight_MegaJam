"""B06 actual-FBX audit: unchanged render/floor/stern, fixed inner faces, fit.

Read-only imports from the archived before FBX and the delivered FBX. All
temporary objects are removed. No Unreal calls. Run twice in Blender.
"""
import bpy,bmesh,sys,os,json,re,hashlib,itertools,io,contextlib,math
from mathutils import Vector
from mathutils.bvhtree import BVHTree
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_b06 as b6
ROOT=r"D:\Projects\LakeFight\Art\Export"


def canonical(name):return re.sub(r"\.\d+$","",name)


def load(path):
    prior=set(bpy.data.objects)
    with contextlib.redirect_stdout(io.StringIO()):bpy.ops.import_scene.fbx(filepath=path)
    obs=[o for o in bpy.data.objects if o not in prior]
    assert all(o.type=="MESH" for o in obs)
    return {canonical(o.name):o for o in obs}


def snapshot(ob):
    ob.data.calc_loop_triangles()
    materials=[]
    for m in ob.data.materials:
        node=next(n for n in m.node_tree.nodes if n.type=="BSDF_PRINCIPLED")
        materials.append(dict(name=canonical(m.name),colour=list(node.inputs["Base Color"].default_value),
            roughness=node.inputs["Roughness"].default_value,metallic=node.inputs["Metallic"].default_value))
    return dict(vertices=[list(ob.matrix_world@v.co) for v in ob.data.vertices],
        faces=[list(p.vertices) for p in ob.data.polygons],material_indices=[p.material_index for p in ob.data.polygons],
        smooth=[p.use_smooth for p in ob.data.polygons],materials=materials,
        uv_layers=[dict(name=layer.name,uv=[list(v.uv) for v in layer.data]) for layer in ob.data.uv_layers],
        colour_attributes=[(a.name,a.domain,a.data_type,len(a.data)) for a in ob.data.color_attributes],
        tris=len(ob.data.loop_triangles),origin=list(ob.matrix_world.translation))


def polygon_area(points,normal):
    return abs(sum((a.cross(b) for a,b in zip(points,points[1:]+points[:1])),Vector()).dot(normal))*.5


def convex(ob):
    assert not ob.data.materials
    assert len(ob.data.vertices)<=24
    bm=bmesh.new();bm.from_mesh(ob.data)
    bmesh.ops.transform(bm,matrix=ob.matrix_world,verts=list(bm.verts));bm.normal_update()
    assert all(e.is_manifold for e in bm.edges),ob.name
    assert all(f.calc_area()>1e-10 for f in bm.faces),ob.name
    violation=max(f.normal.dot(v.co-f.calc_center_median()) for f in bm.faces for v in bm.verts)
    assert violation<2e-5,(ob.name,violation)
    volume=bm.calc_volume(signed=True);assert volume>0,(ob.name,volume)
    bm.free();ob.data.calc_loop_triangles()
    return dict(vertices=len(ob.data.vertices),triangles=len(ob.data.loop_triangles),
        convex=True,max_plane_violation_m=violation,volume_m3=volume,material_slots=0)


def outline_world(ob):
    # FBX round-trip applies the same axis/scale convention to each mesh. Bake
    # only the temporary audit copy to feed world geometry to the outline test.
    me=ob.data.copy()
    me.transform(ob.matrix_world)
    temp=bpy.data.objects.new("B06_Outline_Audit",me)
    result=b6.hull_outline(temp)
    bpy.data.objects.remove(temp,do_unlink=True);bpy.data.meshes.remove(me)
    return result


def envelope_fit(body,pieces):
    xs=sorted(set(x for p in [body]+pieces for x,y in p))
    sample=set(xs)
    # Both envelopes are piecewise linear. Include switches between any pair
    # of overlapping collider outlines so signed max/min are not grid guesses.
    for a,b in zip(xs,xs[1:]):
        if b-a<1e-7:continue
        q1=a+(b-a)*.25;q2=a+(b-a)*.75
        for sign in [-1,1]:
            lines=[]
            for p in pieces:
                ya=b6.outline_y(p,q1,sign);yb=b6.outline_y(p,q2,sign)
                if ya is not None and yb is not None:
                    slope=(yb-ya)/(q2-q1);lines.append((slope,ya-slope*q1))
            for (m,c),(n,d) in itertools.combinations(lines,2):
                if abs(m-n)>1e-10:
                    x=(d-c)/(m-n)
                    if a<x<b:sample.add(x)
    samples=[]
    for sign in [-1,1]:
        for x in sorted(sample):
            target=b6.outline_y(body,x,sign)
            if target is None:continue
            values=[b6.outline_y(p,x,sign) for p in pieces];values=[v for v in values if v is not None]
            assert values,("uncovered hull-outline station",x,sign)
            samples.append(dict(x_m=x,side="port" if sign<0 else "starboard",visible_abs_y_m=target,
                collision_abs_y_m=max(values),signed_error_m=max(values)-target))
    worst=max(samples,key=lambda p:abs(p["signed_error_m"]))
    assert abs(worst["signed_error_m"])<=.02,worst
    return dict(metric="top-down outside envelope, visible wood hull/gunwale; metal hardware excluded",
        max_absolute_gap_m=abs(worst["signed_error_m"]),worst=worst,
        maximum_outward_m=max(p["signed_error_m"] for p in samples),
        maximum_inward_m=max(0,-min(p["signed_error_m"] for p in samples)),
        critical_samples=len(samples))


def inner_proof(old,new,name,index,children,sign):
    source=old[f"UCX_{name}_{index:02d}"]
    points=[source.matrix_world@v.co for v in source.data.vertices]
    ids=[0,1,4,5] if sign>0 else [2,3,6,7]
    fixed=[points[i] for i in ids]
    normal=(fixed[1]-fixed[0]).cross(fixed[2]-fixed[0]).normalized()
    if normal.y*sign<0:normal.negate()
    d=normal.dot(fixed[0])
    original_area=polygon_area([fixed[i] for i in [0,1,3,2]],normal)
    allverts=[];new_area=0;inward=0
    for child in children:
        ob=new[f"UCX_{name}_{child:02d}"]
        vs=[ob.matrix_world@v.co for v in ob.data.vertices]
        allverts+=vs;inward=min(inward,min(normal.dot(v)-d for v in vs))
        ob.data.calc_loop_triangles()
        for t in ob.data.loop_triangles:
            tri=[vs[i] for i in t.vertices]
            if max(abs(normal.dot(v)-d) for v in tri)<2e-6:
                new_area+=(tri[1]-tri[0]).cross(tri[2]-tri[0]).length/2
    displacement=max(min((v-fixedpoint).length for v in allverts) for fixedpoint in fixed)
    assert displacement<1e-6,(name,index,displacement)
    assert inward>-1e-6,(name,index,inward)
    assert abs(new_area-original_area)<2e-6,(name,index,original_area,new_area)
    return dict(original_piece=index,replacement_pieces=children,normal=list(normal),offset=d,
        max_original_corner_displacement_m=displacement,max_inward_plane_intrusion_m=max(0,-inward),
        old_inner_face_area_m2=original_area,new_inner_face_area_m2=new_area,
        analytic_inner_y_at_z085_m={str(x):(d-normal.x*x-normal.z*.085)/normal.y for x in [0,1.185,1.45]})


runs=json.load(open(os.path.join(ROOT,"B06_runs.json")))
assert runs["consecutive_runs_verified"]==2 and runs["runs"][0]==runs["runs"][1]
result={}
for name in ["SM_LF_Rowboat","SM_LF_Rowboat_Blue","SM_LF_Rowboat_Green","SM_LF_Rowboat_Yellow"]:
    old=load(os.path.join(ROOT,"_before_B06",name+".fbx"))
    new=load(os.path.join(ROOT,name+".fbx"))
    try:
        expected={name}|{f"UCX_{name}_{i:02d}" for i in range(12)}
        assert set(new)==expected,(name,set(new))
        render_old=snapshot(old[name]);render_new=snapshot(new[name])
        assert render_old==render_new,(name,"render changed")
        assert render_new["tris"]==1214
        for index in [0,5]:assert snapshot(old[f"UCX_{name}_{index:02d}"])==snapshot(new[f"UCX_{name}_{index:02d}"]),(name,index)
        pieces={i:convex(new[f"UCX_{name}_{i:02d}"]) for i in range(12)}
        inner={i:inner_proof(old,new,name,i,children,sign) for i,children,sign in [(1,[1,6],-1),(2,[2,7],1),(3,[3,8],-1),(4,[4,9],1)]}
        floor_min_x=min(v[0] for v in snapshot(old[f"UCX_{name}_00"])["vertices"])
        aft_max_x=max(v[0] for i in [10,11] for v in snapshot(new[f"UCX_{name}_{i:02d}"])["vertices"])
        assert aft_max_x<floor_min_x-.01
        body=outline_world(new[name])
        new_profiles=[outline_world(new[f"UCX_{name}_{i:02d}"]) for i in range(12)]
        old_profiles=[outline_world(old[f"UCX_{name}_{i:02d}"]) for i in range(6)]
        fit=envelope_fit(body,new_profiles)
        rows=[]
        for sign in [-1,1]:
            for label,x in [("amidships",0),("three-quarter forward",1.185),("bow",1.45),("near stem",1.58)]:
                before=[b6.outline_y(p,x,sign) for p in old_profiles];before=[y for y in before if y is not None]
                after=[b6.outline_y(p,x,sign) for p in new_profiles];after=[y for y in after if y is not None]
                rows.append(dict(station=label,x_m=x,side="port" if sign<0 else "starboard",visible_abs_y_m=b6.outline_y(body,x,sign),
                    before_outer_abs_y_m=max(before) if before else None,after_outer_abs_y_m=max(after)))
        result[name]=dict(render_tris_before=1214,render_tris_after=1214,render_geometry_and_materials_identical=True,
            render_signature_sha256=hashlib.sha256(json.dumps(render_new,sort_keys=True).encode()).hexdigest(),
            floor_00_identical=True,stern_05_identical=True,ucx_pieces=pieces,
            ucx_count=12,ucx_tris_before=72,ucx_tris_after=sum(p["triangles"] for p in pieces.values()),
            maximum_ucx_vertices=max(p["vertices"] for p in pieces.values()),inner_faces=inner,
            added_aft_pieces_behind_original_floor=True,aft_extension_max_x_m=aft_max_x,original_floor_min_x_m=floor_min_x,
            fit=fit,outline_y_table=rows,fbx_sha256=hashlib.sha256(open(os.path.join(ROOT,name+".fbx"),"rb").read()).hexdigest())
        print("B06",name,"PASS",result[name]["ucx_tris_after"],"UCX tris; max",result[name]["maximum_ucx_vertices"],"verts; outline gap",fit["max_absolute_gap_m"]*100,"cm")
    finally:
        for ob in list(old.values())+list(new.values()):bpy.data.objects.remove(ob,do_unlink=True)
with open(os.path.join(ROOT,"B06_fbx_audit.json"),"w") as f:json.dump(result,f,indent=2)
REPORTS=result
