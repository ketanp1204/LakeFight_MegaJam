"""Run B10 asset entrypoints twice, audit exported FBXs, compose UI sheet in Blender."""
import sys, runpy, json, io, contextlib, hashlib, importlib
from pathlib import Path
import bpy
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'BlenderScripts'))
import lf_b10 as b
importlib.reload(b)
OUT=ROOT/'Export'

def run_assets(names):
    path=OUT/'B10_runs.json'
    records=json.loads(path.read_text()) if path.exists() else {}
    log=io.StringIO()
    for name in names:
        runs=[]
        for repeat in [1,2]:
            with contextlib.redirect_stdout(log):
                report=runpy.run_path(str(ROOT/'BlenderScripts'/(name+'.py')))['REPORT']
            runs.append(report)
        assert runs[0]==runs[1],name+' repeat mismatch'
        records[name]={'runs':runs,'identical':True}
        print(name,report['triangles'],'tris; twice identical')
    path.write_text(json.dumps(records,indent=2))
    with (OUT/'B10_execution.log').open('a') as f: f.write(log.getvalue())
    return records

def canonical(ob):
    me=ob.data
    return sorted((tuple(sorted(tuple(round(c,4) for c in me.vertices[i].co) for i in p.vertices)),
                   me.materials[p.material_index].name.split('.')[0]) for p in me.polygons)

def audit(names):
    records=json.loads((OUT/'B10_runs.json').read_text())
    scene=bpy.data.scenes.new('B10_FBX_Audit'); bpy.context.window.scene=scene
    checks=[]
    for name in names:
        source=bpy.data.objects[name]; expected=records[name]['runs'][-1]
        bpy.ops.import_scene.fbx(filepath=str(OUT/(name+'.fbx')))
        objects=list(scene.objects)
        assert len(objects)==1 and objects[0].type=='MESH',name
        ob=objects[0]; ob.data.calc_loop_triangles()
        assert len(ob.data.loop_triangles)==expected['triangles'],name
        assert max(abs(a-c) for a,c in zip(ob.dimensions,expected['dimensions_m']))<.0001,name
        assert ob.location.length<1e-6 and max(abs(c-1) for c in ob.scale)<1e-6,name
        assert canonical(ob)==canonical(source),name+' FBX geometry/material mismatch'
        assert all(not p.use_smooth for p in ob.data.polygons),name
        checks.append({'asset':name,'triangles':len(ob.data.loop_triangles),
                       'fbx_geometry_materials_match_0.1mm':True,'origin_zero':True,
                       'scale_identity':True,'single_mesh':True,'flat_shaded':True,
                       'bytes':(OUT/(name+'.fbx')).stat().st_size})
        bpy.data.objects.remove(ob,do_unlink=True)
    bpy.context.window.scene=b.stage(); bpy.data.scenes.remove(scene)
    path=OUT/('B10_mesh_verified.json' if len(names)==10 else 'B10_verified.json')
    result={'blender':bpy.app.version_string,'two_runs_identical':True,'fbx':checks,
            'note':'FBX binary timestamps vary; geometry and per-face palette compared after actual FBX reimport.'}
    path.write_text(json.dumps(result,indent=2)); print(json.dumps(result))
    return result

def contact_sheet():
    names=['ICON_LF_Fish','ICON_LF_FishGold','ICON_LF_Clock','ICON_LF_Paddle']+['ICON_LF_Crew_'+t for t in ['Red','Blue','Green','Yellow']]
    sheet=np.ones((560,1024,4),dtype=np.float32)
    yy,xx=np.indices((560,1024)); checker=((xx//16+yy//16)%2)*.025
    for c,base in enumerate([.065,.085,.11]): sheet[:,:,c]=base+checker
    checks=[]
    for i,name in enumerate(names):
        path=OUT/'UI'/(name+'.png'); im=bpy.data.images.load(str(path),check_existing=False)
        assert tuple(im.size)==(256,256) and im.channels==4,name
        px=np.array(im.pixels[:],dtype=np.float32).reshape(256,256,4); alpha=px[:,:,3]
        assert alpha.min()==0 and alpha.max()>.99,name
        where=np.argwhere(alpha>.01)
        assert where.min(axis=0).min()>2 and where.max(axis=0).max()<253,name
        x=i%4*256; y=(1-i//4)*280+12
        dst=sheet[y:y+256,x:x+256,:3]
        dst[:]=px[:,:,:3]*alpha[:,:,None]+dst*(1-alpha[:,:,None])
        checks.append({'name':name,'rgba_256':True,'transparent':True,'unclipped':True,
                       'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
        bpy.data.images.remove(im)
    im=bpy.data.images.get('B10_ContactSheet') or bpy.data.images.new('B10_ContactSheet',width=1024,height=560,alpha=True)
    im.pixels.foreach_set(sheet.ravel()); im.filepath_raw=str(OUT/'UI'/'B10_ContactSheet.png'); im.file_format='PNG'; im.save()
    (OUT/'B10_UI_verified.json').write_text(json.dumps({'order':names,'checks':checks},indent=2))
    print('Updated eight-icon sheet:',im.filepath_raw)
