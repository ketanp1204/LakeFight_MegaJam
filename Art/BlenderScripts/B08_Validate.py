"""B08: execute every asset twice, check actual FBX and RGBA outputs, build contact sheet.
runpy.run_path(...)[run_assets](names) in short serial MCP calls, then finish().
"""
import sys, runpy, json, hashlib
from pathlib import Path
import bpy
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'BlenderScripts'))
import lf_b08 as b
NAMES=['SM_LF_FishSpot','SM_LF_BankZone','ICON_LF_Fish','ICON_LF_FishGold','ICON_LF_Clock','ICON_LF_Paddle']+['ICON_LF_Crew_'+t for t in ['Red','Blue','Green','Yellow']]
LOG=ROOT/'Export'/'B08_runs.json'

def run_assets(names):
    records=json.loads(LOG.read_text()) if LOG.exists() else {}
    for name in names:
        runs=[]
        for repeat in [1,2]:
            runs.append(runpy.run_path(str(ROOT/'BlenderScripts'/(name+'.py')))['REPORT'])
        assert runs[0]==runs[1],(name,runs)
        records[name]={'runs':runs,'identical':True}
        LOG.write_text(json.dumps(records,indent=2))
        print(name,'TWO RUNS IDENTICAL',runs[-1]['triangles'])

def finish():
    records=json.loads(LOG.read_text()); assert set(records)==set(NAMES)
    scene=b.stage(); audit=[]
    # Import the actual FBXs into a disposable scene, avoiding suffix collisions.
    test=bpy.data.scenes.new('B08_FBX_Audit'); bpy.context.window.scene=test
    for name in NAMES:
        source=bpy.data.objects[name]
        def geometry(ob):
            return sorted((tuple(sorted(tuple(round(c,4) for c in v.co) for v in [ob.data.vertices[i] for i in p.vertices])),ob.data.materials[p.material_index].name.split('.')[0]) for p in ob.data.polygons)
        expected_geometry=geometry(source)
        bpy.ops.import_scene.fbx(filepath=str(ROOT/'Export'/(name+'.fbx')))
        objs=list(test.objects); assert len(objs)==1 and objs[0].type=='MESH'
        ob=objs[0]; ob.data.calc_loop_triangles(); expected=records[name]['runs'][-1]
        assert len(ob.data.loop_triangles)==expected['triangles']
        assert all(abs(a-c)<.0001 for a,c in zip(ob.dimensions,expected['dimensions_m']))
        assert ob.location.length<.00001
        assert not any(o.name.startswith('UCX_') for o in objs)
        # Reimported material names can acquire Blender numeric suffixes.
        mats=[m.name.split('.')[0] for m in ob.data.materials]
        assert mats==expected['materials'],(mats,expected)
        assert geometry(ob)==expected_geometry,name+' exported vertex/face mismatch'
        audit.append({'asset':name,'fbx_triangles':len(ob.data.loop_triangles),'dimensions_match':True,'origin_zero':True,'materials_match':True,'geometry_match_0.1mm':True,'no_ucx':True})
        for o in objs: bpy.data.objects.remove(o,do_unlink=True)
    bpy.context.window.scene=scene; bpy.data.scenes.remove(test)
    sheet=np.ones((560,1024,4),dtype=np.float32)
    yy,xx=np.indices((560,1024)); check=((xx//16+yy//16)%2)*.025
    for ch,base in enumerate([.065,.085,.11]): sheet[:,:,ch]=base+check
    image_checks=[]
    for i,name in enumerate(NAMES[2:]):
        path=ROOT/'Export'/'UI'/(name+'.png')
        im=bpy.data.images.load(str(path),check_existing=False)
        assert tuple(im.size)==(256,256) and im.channels==4
        pixels=np.array(im.pixels[:],dtype=np.float32).reshape(256,256,4)
        alpha=pixels[:,:,3]; assert alpha.min()==0 and alpha.max()>.99
        occupied=np.argwhere(alpha>.01)
        assert occupied[:,0].min()>2 and occupied[:,0].max()<253
        assert occupied[:,1].min()>2 and occupied[:,1].max()<253
        row=1-i//4; x=(i%4)*256; y=row*280+12
        dst=sheet[y:y+256,x:x+256,:3]
        dst[:]=pixels[:,:,:3]*alpha[:,:,None]+dst*(1-alpha[:,:,None])
        image_checks.append({'asset':name,'size':[256,256],'channels':4,'transparent':True,'unclipped':True})
        bpy.data.images.remove(im)
    im=bpy.data.images.get('B08_ContactSheet') or bpy.data.images.new('B08_ContactSheet',width=1024,height=560,alpha=True)
    im.pixels.foreach_set(sheet.ravel()); im.filepath_raw=str(ROOT/'Export'/'UI'/'B08_ContactSheet.png'); im.file_format='PNG'; im.save()
    result={'blender':bpy.app.version_string,'asset_scripts_run_twice':True,'fbx':audit,'png':image_checks,'sheet_order':NAMES[2:],'note':'FBX bytes include export timestamps; determinism checked on full mesh vertices/faces/material assignments plus identical PNG SHA256.'}
    (ROOT/'Export'/'B08_verified.json').write_text(json.dumps(result,indent=2))
    # Save a useful review stage with all assets visible, laid out without altering exported coordinates.
    for i,name in enumerate(NAMES):
        col=bpy.data.collections[name]; col.hide_render=False
        ob=next(iter(col.objects)); ob.location=(i%5*6,i//5*6,0)
    # Exported origins stay zero; these absolute offsets are only the saved review layout.
    bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Export'/'B08_Review.blend'))
    print(json.dumps(result))
