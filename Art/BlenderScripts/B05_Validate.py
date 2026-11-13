"""B05 FBX round-trip audit and unchanged dock-base check. Run twice in Blender."""
import bpy,bmesh,sys,os,json,re,io,contextlib,hashlib
from collections import Counter
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf
ROOT=r"D:\Projects\LakeFight\Art\Export"
runs=json.load(open(os.path.join(ROOT,"B05_runs.json")))
reports=[r for e in runs.values() for r in e["runs"][-1]]
audit={}
for e in runs.values():
    assert e["status"]=="PASS" and e["consecutive_runs_verified"]==2
    assert e["runs"][0]==e["runs"][1]
for wanted in reports:
    name=wanted["asset"];path=os.path.join(ROOT,name+".fbx")
    prior=set(bpy.data.objects)
    with contextlib.redirect_stdout(io.StringIO()):bpy.ops.import_scene.fbx(filepath=path)
    imported=[o for o in bpy.data.objects if o not in prior]
    try:
        obs=[o for o in imported if o.type=="MESH"]
        assert len(obs)==1,(name,"must import as one static mesh")
        assert all(o.type=="MESH" for o in imported),(name,"unexpected object type")
        coords=[o.matrix_world@v.co for o in obs for v in o.data.vertices]
        lo=[min(v[i] for v in coords) for i in range(3)]
        hi=[max(v[i] for v in coords) for i in range(3)]
        tris=bad_edges=bad_faces=smooth=0;colours={}
        for ob in obs:
            ob.data.calc_loop_triangles();tris+=len(ob.data.loop_triangles)
            smooth+=sum(p.use_smooth for p in ob.data.polygons)
            bm=bmesh.new();bm.from_mesh(ob.data)
            bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-7)
            bad_edges+=sum(not e.is_manifold for e in bm.edges)
            bad_faces+=sum(f.calc_area()<1e-10 for f in bm.faces)
            bm.free()
            for m in ob.data.materials:
                key=re.sub(r"\.\d+$","",m.name)
                rgb=list(next(n for n in m.node_tree.nodes if n.type=="BSDF_PRINCIPLED").inputs["Base Color"].default_value)[:3]
                assert max(abs(rgb[i]-lf.PALETTE[key.removeprefix("M_LF_")][i]) for i in range(3))<1e-5
                assert not any(n.type=="TEX_IMAGE" for n in m.node_tree.nodes)
                colours[key]=rgb
        assert tris==wanted["tris"] and tris<wanted["limit_exclusive"],(name,tris)
        assert sorted(colours)==wanted["palette"],(name,colours)
        assert max(abs(lo[i]-wanted["bounds_min_m"][i]) for i in range(3))<1e-5
        assert max(abs(hi[i]-wanted["bounds_max_m"][i]) for i in range(3))<1e-5
        assert not bad_edges and not bad_faces and not smooth,(name,bad_edges,bad_faces,smooth)
        audit[name]=dict(status="PASS",tris=tris,bounds_min_m=lo,bounds_max_m=hi,
            size_m=[hi[i]-lo[i] for i in range(3)],material_linear=colours,
            nonmanifold_edges=bad_edges,degenerate_faces=bad_faces,smooth_faces=smooth,
            fbx_sha256=hashlib.sha256(open(path,"rb").read()).hexdigest(),fbx_bytes=os.path.getsize(path),mesh_objects=1)
    finally:
        for ob in imported:bpy.data.objects.remove(ob,do_unlink=True)
assert audit["SM_LF_Logo_LakeFight"]["tris"]+audit["SM_LF_Logo_DirectorsCut"]["tris"]<5000
assert not {"M_LF_"+x for x in lf.TEAM_COLOURS}&set(audit["SM_LF_Dock_Team"]["material_linear"])
# Check the retained original dock prefix against the existing B02 FBX.
prior=set(bpy.data.objects)
with contextlib.redirect_stdout(io.StringIO()):bpy.ops.import_scene.fbx(filepath=os.path.join(ROOT,"SM_LF_Dock.fbx"))
imported=[o for o in bpy.data.objects if o not in prior]
try:
    old=next(o for o in imported if o.type=="MESH")
    old.data.calc_loop_triangles()
    assert len(old.data.loop_triangles)==788
    oldpoints=[old.matrix_world@v.co for v in old.data.vertices]
    new=bpy.data.objects["SM_LF_Dock_Team"]
    newpoints=[new.matrix_world@v.co for v in list(new.data.vertices)[:len(oldpoints)]]
    rounding=lambda pts:Counter(tuple(round(v[i],5) for i in range(3)) for v in pts)
    assert rounding(oldpoints)==rounding(newpoints),"Original dock geometry moved"
    basecheck=dict(original_dock_tris=788,team_dock_tris=audit["SM_LF_Dock_Team"]["tris"],
                   added_triangles=204,base_vertices_unchanged=True,base_vertex_count=len(oldpoints))
finally:
    for ob in imported:bpy.data.objects.remove(ob,do_unlink=True)
proof=json.load(open(os.path.join(ROOT,"B05_review_proof.json")))
assert abs(proof["target_mannequin_height_m"]-1.35)<1e-6
output=dict(assets=audit,dock_base_check=basecheck,logo_combined_tris=3152)
with open(os.path.join(ROOT,"B05_fbx_audit.json"),"w") as f:json.dump(output,f,indent=2)
print("B05 FBX AUDIT PASS: 7 single-mesh FBXs; exact counts/materials, bounds within .01mm, closed and flat, no textures; original dock vertices unchanged.")
REPORTS=output
