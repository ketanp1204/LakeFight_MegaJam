"""Round-trip all 12 B04 FBXs plus the eight palette-only re-exports.

Run in Blender twice; transient imports are removed; all output under Art/Export.
"""
import bpy,bmesh,sys,os,json,re,hashlib,io,contextlib
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf
ROOT=r"D:\Projects\LakeFight\Art\Export"
runs=json.load(open(os.path.join(ROOT,"B04_runs.json")))
palette_fix=json.load(open(os.path.join(ROOT,"B04_palette_fix.json")))
expected={r["asset"]:r for e in runs.values() for r in e["runs"][-1]}
for e in runs.values():
    assert e["status"]=="PASS" and e["consecutive_runs_verified"]==2
    assert e["runs"][0]==e["runs"][1]
for name,baseline in palette_fix["baseline_FBX"].items():expected[name]=baseline
audit={}
for name,wanted in expected.items():
    path=os.path.join(ROOT,name+".fbx")
    prior=set(bpy.data.objects)
    with contextlib.redirect_stdout(io.StringIO()):bpy.ops.import_scene.fbx(filepath=path)
    imported=[o for o in bpy.data.objects if o not in prior]
    try:
        obs=[o for o in imported if o.type=="MESH"]
        coords=[o.matrix_world@v.co for o in obs for v in o.data.vertices]
        lo=[min(v[i] for v in coords) for i in range(3)]
        hi=[max(v[i] for v in coords) for i in range(3)]
        tris=0;bad_edges=0;bad_faces=0;smooth=0;colours={}
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
                palette_key=key.removeprefix("M_LF_")
                node=next(n for n in m.node_tree.nodes if n.type=="BSDF_PRINCIPLED")
                rgb=list(node.inputs["Base Color"].default_value)[:3]
                assert max(abs(rgb[i]-lf.PALETTE[palette_key][i]) for i in range(3))<1e-5,(name,key,rgb)
                assert not any(n.type=="TEX_IMAGE" for n in m.node_tree.nodes)
                colours[key]=rgb
        assert tris==wanted["tris"],(name,tris,wanted["tris"])
        assert max(abs(lo[i]-wanted["bounds_min_m"][i]) for i in range(3))<1e-5,(name,lo)
        assert max(abs(hi[i]-wanted["bounds_max_m"][i]) for i in range(3))<1e-5,(name,hi)
        assert not bad_edges and not bad_faces and not smooth,(name,bad_edges,bad_faces,smooth)
        if "palette" in wanted:
            assert sorted(colours)==wanted["palette"]
            assert tris<600
        audit[name]=dict(status="PASS",tris=tris,bounds_min_m=lo,bounds_max_m=hi,
            size_m=[hi[i]-lo[i] for i in range(3)],material_linear=colours,
            nonmanifold_edges=bad_edges,degenerate_faces=bad_faces,smooth_faces=smooth,
            fbx_bytes=os.path.getsize(path),fbx_sha256=hashlib.sha256(open(path,"rb").read()).hexdigest())
    finally:
        for ob in imported:bpy.data.objects.remove(ob,do_unlink=True)
review=json.load(open(os.path.join(ROOT,"B04_review_proof.json")))
assert abs(review["proxy_height_m"]-1.35)<1e-6
assert review["target"]=="UE5 Manny at 0.75 scale"
with open(os.path.join(ROOT,"B04_fbx_audit.json"),"w") as f:json.dump(audit,f,indent=2)
print("B04 FBX AUDIT PASS:",len(audit),"FBXs; twelve props <600 tris, matching target-size bounds, closed/flat, correct updated shader RGB.")
REPORTS=audit
