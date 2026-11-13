"""B06 convex segments follow the gunwale outline and eleven-degree side flare.

Only UCX mesh data changes. Original inner faces are partitioned without moving
their planes; an aft cheek covers the old gap behind the side wall. Outline
measurements exclude metal accessories. No change to floor, stern or rendering.
"""
import bpy,bmesh,math,json
from mathutils import Vector
from mathutils.bvhtree import BVHTree


def hull_tree(hull):
    hull.data.calc_loop_triangles()
    faces=[tuple(t.vertices) for t in hull.data.loop_triangles
           if hull.data.materials[hull.data.polygons[t.polygon_index].material_index].name.split(".")[0]!="M_LF_Metal"]
    return BVHTree.FromPolygons([v.co for v in hull.data.vertices],faces,all_triangles=True)


def surface_y(tree,x,z,sign=1):
    hit=tree.ray_cast(Vector((x,sign*2,z)),Vector((0,-sign,0)),4.)
    return hit[0] if hit[0] is not None else None


def hull_outline(hull):
    used={i for p in hull.data.polygons if not hull.data.materials or hull.data.materials[p.material_index].name.split(".")[0]!="M_LF_Metal" for i in p.vertices}
    points=sorted(set((float(hull.data.vertices[i].co.x),float(hull.data.vertices[i].co.y)) for i in used))
    def cross(a,b,c):return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    lower=[];upper=[]
    for p in points:
        while len(lower)>=2 and cross(lower[-2],lower[-1],p)<=1e-10:lower.pop()
        lower.append(p)
    for p in reversed(points):
        while len(upper)>=2 and cross(upper[-2],upper[-1],p)<=1e-10:upper.pop()
        upper.append(p)
    return lower[:-1]+upper[:-1]


def outline_y(outline,x,sign=1):
    values=[]
    for a,b in zip(outline,outline[1:]+outline[:1]):
        if min(a[0],b[0])-1e-7<=x<=max(a[0],b[0])+1e-7:
            if abs(a[0]-b[0])<1e-9:values.extend([sign*a[1],sign*b[1]])
            else:values.append(sign*(a[1]+(b[1]-a[1])*(x-a[0])/(b[0]-a[0])))
    return max(values) if values else None


def clip_x(points,bound,keep_above):
    clipped=[]
    for a,b in zip(points,points[1:]+points[:1]):
        da=(a.x-bound)*(1 if keep_above else -1)
        db=(b.x-bound)*(1 if keep_above else -1)
        if da>=-1e-7:clipped.append(a.copy())
        if (da<0)!=(db<0):clipped.append(a.lerp(b,da/(da-db)))
    clean=[]
    for p in clipped:
        if not clean or (p-clean[-1]).length>1e-7:clean.append(p)
    if len(clean)>1 and (clean[0]-clean[-1]).length<1e-7:clean.pop()
    return clean


def fit_wall(ob,hull,sign,bow,sheer,bounds=None,exterior_inset=0):
    original=[v.co.copy() for v in ob.data.vertices]
    inner_ids=[0,1,4,5] if sign>0 else [2,3,6,7]
    inner=[original[i] for i in inner_ids]
    normal=(inner[1]-inner[0]).cross(inner[2]-inner[0]).normalized()
    if normal.dot(Vector((0,sign,0)))<0:normal.negate()
    offset=normal.dot(inner[0])
    outline=hull_outline(hull)
    source_inner=[inner[i] for i in [0,1,3,2]]
    xmin,xmax=bounds or (min(v.x for v in inner),max(v.x for v in inner))
    inner=clip_x(clip_x(source_inner,xmin,True),xmax,False)
    assert len(inner)>=3,(ob.name,bounds)
    xs=sorted(set([xmin,xmax,(xmin+xmax)/2]+[x for x,y in outline if xmin<x<xmax]))
    points=inner[:]
    for x in xs:
        width=outline_y(outline,x,sign)
        if width is None:continue
        width=max(0,width-exterior_inset)
        # The upper outside edge follows the visible gunwale silhouette. Its
        # height may taper at the stem to retain the protected inner plane.
        roof=min(sheer(x)+.02,(normal.x*x+normal.y*sign*width-offset-.001)/(-normal.z))
        low=-.14
        if bow and x>1.50:low=max(low,-.14+(x-1.50)/(1.59896-1.50)*.39)
        low=min(low,roof-.025)
        for z in [low,roof]:
            y=sign*max(0,width+math.tan(math.radians(11))*(z-roof))
            hit=Vector((x,y,z))
            if normal.dot(hit)>=offset+.0001:points.append(hit)
    bm=bmesh.new()
    verts=[bm.verts.new(v) for v in points]
    bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
    result=bmesh.ops.convex_hull(bm,input=list(bm.verts),use_existing_faces=False)
    unused=[v for v in result["geom_interior"]+result["geom_unused"] if isinstance(v,bmesh.types.BMVert) and v.is_valid]
    if unused:bmesh.ops.delete(bm,geom=list(set(unused)),context="VERTS")
    bmesh.ops.dissolve_limit(bm,angle_limit=.0003,verts=list(bm.verts),edges=list(bm.edges),use_dissolve_boundaries=False,delimit=set())
    bmesh.ops.dissolve_degenerate(bm,dist=1e-6,edges=list(bm.edges))
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    bm.normal_update()
    assert all(e.is_manifold for e in bm.edges)
    assert all(f.calc_area()>1e-10 for f in bm.faces)
    violation=max(f.normal.dot(v.co-f.calc_center_median()) for f in bm.faces for v in bm.verts)
    assert violation<1e-5,(ob.name,violation)
    for fixed in inner:
        assert any((v.co-fixed).length<1e-7 for v in bm.verts),(ob.name,"inner corner lost")
    assert min(normal.dot(v.co)-offset for v in bm.verts)>-1e-6
    bm.to_mesh(ob.data);bm.free();ob.data.update()
    for p in ob.data.polygons:p.use_smooth=False
    ob.data.calc_loop_triangles()
    return dict(source_inner_face_m=[list(v) for v in source_inner],original_inner_vertices_m=[list(v) for v in inner],inner_normal=list(normal),
                inner_plane_offset=offset,inner_vertices_preserved=True,
                inner_plane_preserved=True,vertices=len(ob.data.vertices),tris=len(ob.data.loop_triangles),
                convexity_max_plane_violation_m=violation,segment_x_bounds_m=[xmin,xmax],exterior_inset_m=exterior_inset)


def aft_cheek_source(ob,hull,sign):
    original=[v.co.copy() for v in ob.data.vertices]
    ids=[0,1,4,5] if sign>0 else [2,3,6,7]
    inner=[original[i] for i in ids]
    normal=(inner[1]-inner[0]).cross(inner[2]-inner[0]).normalized()
    if normal.y*sign<0:normal.negate()
    d=normal.dot(inner[0])
    outline=hull_outline(hull)
    stern_x=min(p[0] for p in outline);join_x=min(v.x for v in inner)
    ztop=max(v.z for v in inner);zref=.085
    join_y=sign*(d-normal.z*zref)/normal.y
    stern_y=.91*outline_y(outline,stern_x,sign)-math.tan(math.radians(11))*(ztop-zref)
    slope=(join_y-stern_y)/(join_x-stern_x)
    for i,v in enumerate(ob.data.vertices):
        x=stern_x if original[i].x<(min(p.x for p in original)+max(p.x for p in original))/2 else join_x
        y=stern_y+slope*(x-stern_x)+math.tan(math.radians(11))*(original[i].z-zref)
        if i not in ids:y+=.04
        v.co=(x,sign*y,original[i].z)
    ob.data.update()
    return stern_x,join_x


def fit_collision_set(col,hull,name,sheer):
    """Twelve convex shapes: floor/stern retained, three sides + two bows/side."""
    results={}
    side_cut=-.15;bow_cut=1.35
    for sign,side,bow in [(-1,1,3),(1,2,4)]:
        old=col.objects[f"UCX_{name}_{side:02d}"]
        template=old.data.copy()
        xmin=min(v.co.x for v in old.data.vertices);xmax=max(v.co.x for v in old.data.vertices)
        for index,bounds in [(side,(xmin,side_cut)),(6 if sign<0 else 7,(side_cut,xmax))]:
            if index==side:
                ob=old
                ob.data=template.copy()
            else:
                ob=bpy.data.objects.new(f"UCX_{name}_{index:02d}",template.copy());col.objects.link(ob)
            ob.data.name=ob.name+"_Mesh"
            results[index]=fit_wall(ob,hull,sign,False,sheer,bounds)
            results[index].update(replaces_original=side,role="side aft" if index==side else "side forward")
        index=10 if sign<0 else 11
        ob=bpy.data.objects.new(f"UCX_{name}_{index:02d}",template.copy());col.objects.link(ob)
        bounds=aft_cheek_source(ob,hull,sign)
        results[index]=fit_wall(ob,hull,sign,False,sheer,bounds)
        results[index].update(new_aft_extension=True,replaces_original=None,role="new aft cheek; joins original side inner plane at X=-1.28")
        bpy.data.meshes.remove(template)
        old=col.objects[f"UCX_{name}_{bow:02d}"]
        template=old.data.copy()
        xmin=min(v.co.x for v in old.data.vertices)
        for index,bounds in [(bow,(xmin,bow_cut)),(8 if sign<0 else 9,(bow_cut,1.59896))]:
            if index==bow:
                ob=old;ob.data=template.copy()
            else:
                ob=bpy.data.objects.new(f"UCX_{name}_{index:02d}",template.copy());col.objects.link(ob)
            ob.data.name=ob.name+"_Mesh"
            results[index]=fit_wall(ob,hull,sign,True,sheer,bounds)
            results[index].update(replaces_original=bow,role="bow rear" if index==bow else "bow stem")
        bpy.data.meshes.remove(template)
    for index,result in results.items():
        assert result["vertices"]<=24,(name,index,result["vertices"])
        ob=col.objects[f"UCX_{name}_{index:02d}"]
        ob["LF_CollisionRole"]=result["role"]
    return {f"UCX_{name}_{index:02d}":results[index] for index in sorted(results)}
