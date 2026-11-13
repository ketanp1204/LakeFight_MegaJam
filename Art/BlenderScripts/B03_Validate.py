"""Round-trip the delivered FBXs through Blender and compare the two-run evidence.

Does not rebuild assets. Owns only transient imported objects; writes under Art/Export.
Run twice through execute_blender_code after the asset scripts have passed twice.
"""
import bpy, bmesh, sys, json, os, re, hashlib, contextlib, io
from mathutils import Vector
ROOT=r"D:\Projects\LakeFight\Art\Export"
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
with open(os.path.join(ROOT,"B03_runs.json")) as f: runs=json.load(f)
with open(os.path.join(ROOT,"B022_verified.json")) as f: fixed=json.load(f)
reports={name:entry["runs"][-1] for name,entry in runs.items()}
reports.update({name:entry["runs"][-1] for name,entry in fixed.items() if name!="SM_LF_ReedBed"})
audit={}
for name,expected in reports.items():
    prior=set(bpy.data.objects)
    with contextlib.redirect_stdout(io.StringIO()):
        bpy.ops.import_scene.fbx(filepath=os.path.join(ROOT,name+".fbx"))
    imported=[o for o in bpy.data.objects if o not in prior]
    try:
        meshes=[o for o in imported if o.type=="MESH"]
        coords=[o.matrix_world@v.co for o in meshes for v in o.data.vertices]
        lo=[min(v[i] for v in coords) for i in range(3)]
        hi=[max(v[i] for v in coords) for i in range(3)]
        tris=0
        bad_edges=0
        bad_faces=0
        smooth_faces=0
        for ob in meshes:
            ob.data.calc_loop_triangles()
            tris+=len(ob.data.loop_triangles)
            smooth_faces+=sum(p.use_smooth for p in ob.data.polygons)
            bm=bmesh.new()
            bm.from_mesh(ob.data)
            # FBX may split normal/material vertices; weld only in the audit copy.
            bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-7)
            bad_edges+=sum(not e.is_manifold for e in bm.edges)
            bad_faces+=sum(f.calc_area()<1e-10 for f in bm.faces)
            bm.free()
        palette=sorted({re.sub(r"\.\d+$","",m.name) for o in meshes for m in o.data.materials})
        assert tris==expected["tris"],(name,tris,expected["tris"])
        assert max(abs(lo[i]-expected["bounds_min_m"][i]) for i in range(3))<1e-5,(name,lo)
        assert max(abs(hi[i]-expected["bounds_max_m"][i]) for i in range(3))<1e-5,(name,hi)
        assert palette==expected["palette"],(name,palette,expected["palette"])
        assert not bad_edges and not bad_faces and not smooth_faces,(name,bad_edges,bad_faces,smooth_faces)
        assert not any(n.type=="TEX_IMAGE" for o in meshes for m in o.data.materials if m.use_nodes for n in m.node_tree.nodes)
        audit[name]=dict(status="PASS",tris=tris,bounds_min_m=lo,bounds_max_m=hi,
                         nonmanifold_edges=bad_edges,degenerate_faces=bad_faces,smooth_faces=smooth_faces,
                         palette=palette,fbx_bytes=os.path.getsize(os.path.join(ROOT,name+".fbx")),
                         fbx_sha256=hashlib.sha256(open(os.path.join(ROOT,name+".fbx"),"rb").read()).hexdigest())
    finally:
        for ob in imported: bpy.data.objects.remove(ob,do_unlink=True)
for name,entry in runs.items():
    assert entry["status"]=="PASS" and entry["consecutive_runs_verified"]==2,name
    assert entry["runs"][0]==entry["runs"][1],name
with open(os.path.join(ROOT,"B03_fbx_audit.json"),"w") as f: json.dump(audit,f,indent=2)
print("FBX ROUND-TRIP PASS:",len(audit),"assets; exact tris/materials, bounds within 0.01mm, closed, flat, no textures.")
REPORTS=audit
