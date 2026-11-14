"""Serial B11 repeat-run checks and actual exported FBX hierarchy/geometry audit."""
import sys, runpy, importlib, json, io, contextlib
from pathlib import Path
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1]; OUT=ROOT/'Export'
sys.path.insert(0,str(ROOT/'BlenderScripts'))
import lf_b11 as b
importlib.reload(b)

def run_assets(names=None):
    names=b.NAMES if names is None else names
    path=OUT/'B11_runs.json'; records=json.loads(path.read_text()) if path.exists() else {}
    log=io.StringIO()
    for name in names:
        runs=[]
        for repeat in [1,2]:
            with contextlib.redirect_stdout(log):
                runs.append(runpy.run_path(str(ROOT/'BlenderScripts'/(name+'.py')))['REPORT'])
        assert runs[0]==runs[1],name+' repeat mismatch'
        records[name]={'runs':runs,'identical':True}
        print(name,runs[-1]['triangles'],'tris; two matching runs')
    path.write_text(json.dumps(records,indent=2))
    with (OUT/'B11_execution.log').open('a') as f: f.write(log.getvalue())
    return records

def canonical(ob):
    me=ob.data
    return sorted((tuple(sorted(tuple(round(c,5) for c in me.vertices[i].co) for i in p.vertices)),
                   me.materials[p.material_index].name.split('.')[0]) for p in me.polygons)

def audit():
    records=json.loads((OUT/'B11_runs.json').read_text())
    assert set(records)==set(b.NAMES)
    scene=bpy.data.scenes.new('B11_FBX_Audit'); bpy.context.window.scene=scene
    checks=[]
    for name in b.NAMES:
        expected=records[name]['runs'][-1]
        bpy.ops.import_scene.fbx(filepath=str(OUT/(name+'.fbx')))
        objects=list(scene.objects); meshes=[o for o in objects if o.type=='MESH']
        assert len(objects)==len(meshes)==len(expected['parts']),name
        part_checks=[]
        for part in expected['parts']:
            ob=next(o for o in meshes if o.name.split('.')[0]==part['name'])
            source=bpy.data.objects[part['name']]
            ob.data.calc_loop_triangles()
            assert len(ob.data.loop_triangles)==part['triangles'],part
            assert canonical(ob)==canonical(source),name+' geometry/material mismatch'
            assert max(abs(a-c) for a,c in zip(ob.location,part['origin_local_m']))<1e-5,part
            parent=ob.parent.name.split('.')[0] if ob.parent else None
            assert parent==part['parent'],part
            assert max(abs(c-1) for c in ob.scale)<1e-6 and max(abs(c) for c in ob.rotation_euler)<1e-6,part
            assert all(not p.use_smooth for p in ob.data.polygons),part
            part_checks.append({'name':part['name'],'triangles':part['triangles'],
                                'local_geometry_materials_match_0.01mm':True,
                                'parent':parent,'pivot_local_m':[round(c,7) for c in ob.location],
                                'identity_rotation_scale':True,'flat_shaded':True})
        checks.append({'asset':name,'total_triangles':expected['triangles'],
                       'budget_passed':True,'parts':part_checks,'bytes':(OUT/(name+'.fbx')).stat().st_size})
        for ob in objects: bpy.data.objects.remove(ob,do_unlink=True)
    bpy.context.window.scene=b.stage(); bpy.data.scenes.remove(scene)
    result={'blender':bpy.app.version_string,'all_asset_scripts_run_twice':True,
            'all_final_runs_identical':True,'fbx':checks,
            'note':'FBX embeds timestamps: determinism compares geometry/materials/hierarchy rather than file bytes.'}
    flying_total=sum(records[n]['runs'][-1]['triangles'] for n in b.FLYING[:3])
    assert flying_total<150
    result['flying_assembled_triangles']=flying_total
    result['flying_glide_triangles']=records['SM_LF_Bird_Glide']['runs'][-1]['triangles']
    flying_vertices=[]
    for name in b.FLYING[:3]:
        offset=Vector(b.SHOULDERS[name[-1]]) if '_Wing_' in name else Vector((0,0,0))
        flying_vertices.extend(v.co+offset for v in bpy.data.objects[name].data.vertices)
    span=max(v.y for v in flying_vertices)-min(v.y for v in flying_vertices)
    assert abs(span-.9)<1e-6,span
    result['flying_assembled_span_m']=round(span,6)
    insect_totals={label:sum(records[n]['runs'][-1]['triangles'] for n in names)
                   for label,names in {'dragonfly':b.INSECTS[:2],'butterfly':b.INSECTS[2:]}.items()}
    assert all(n<100 for n in insect_totals.values()),insect_totals
    result['insect_assembled_triangles']=insect_totals
    attachments={'units':'metres; multiply positions by 100 for Unreal centimetres',
                 'forward':'+X','left':'-Y','up':'+Z',
                 'swan':{'body':'SM_LF_Swan','child':'SM_LF_Swan_HeadNeck',
                         'child_relative_pivot_m':[.1210526,0,.1105263],
                         'neutral_rotation_degrees':[0,0,0]},
                 'flying_bird':{'body':'SM_LF_Bird_Body','assembled_triangles':flying_total,
                         'level_span_m':round(span,6),
                         'children':[{'mesh':'SM_LF_Bird_Wing_'+side,'relative_shoulder_m':list(b.SHOULDERS[side]),
                                      'neutral_rotation_degrees':[0,0,0],
                                      'up_rotation_about_local_X_degrees':-42 if side=='L' else 42,
                                      'down_rotation_about_local_X_degrees':32 if side=='L' else -32}
                                     for side in ['L','R']],
                         'glide':'SM_LF_Bird_Glide','glide_triangles':result['flying_glide_triangles'],
                         'glide_dihedral_degrees':6}}
    attachments['insects']={'authored_scale_multiplier':1.5,
        'dragonfly':{'body':'SM_LF_Dragonfly','wings':'SM_LF_Dragonfly_Wings',
                     'wings_relative_hinge_m':[.0255,0,0],'assembled_triangles':insect_totals['dragonfly'],
                     'body_length_m':.12,'wing_planes':4},
        'butterfly':{'body':'SM_LF_Butterfly_Body','assembled_triangles':insect_totals['butterfly'],
                     'children':[{'mesh':'SM_LF_Butterfly_Wing_'+side,'relative_hinge_m':list(b.BUTTERFLY_HINGES[side])} for side in ['L','R']],
                     'flap_axis':'local X; left up negative / right up positive'}}
    (OUT/'B11_ATTACHMENTS.json').write_text(json.dumps(attachments,indent=2))
    (OUT/'B11_verified.json').write_text(json.dumps(result,indent=2))
    print(json.dumps({'blender':result['blender'],'fbx_outputs_audited':len(checks),
                      'all_final_runs_identical':True,'flying_assembled_triangles':flying_total,
                      'glide_triangles':result['flying_glide_triangles'],
                      'insect_assembled_triangles':insect_totals}))
    return result
