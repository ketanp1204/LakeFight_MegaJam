"""B12a: render original B05 title/ribbon as transparent key-art overlays in Blender.

Run via Blender MCP. Only this task's scene/collections/images are rebuilt.
No original meshes, materials, FBXs or review scenes are changed.
"""
import bpy
import numpy as np
import math, json, struct, hashlib
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Export' / 'KeyArt'
OUT.mkdir(parents=True, exist_ok=True)
FORMATS = {
    '630x500': (630, 500, (32, 20, 566, 210), (24, 238, 582, 238)),
    '960x300': (960, 300, (22, 30, 426, 240), (478, 18, 464, 264)),
    '1920x1080': (1920, 1080, (480, 36, 960, 400), (96, 460, 1728, 566)),
}

def clean_png(path):
    raw = path.read_bytes()
    result = raw[:8]
    at = 8
    while at < len(raw):
        length = struct.unpack('>I', raw[at:at+4])[0]
        end = at + length + 12
        if raw[at+4:at+8] not in (b'tEXt', b'zTXt', b'iTXt', b'tIME'):
            result += raw[at:end]
        at = end
    path.write_bytes(result)

def scene_new(name):
    old = bpy.data.scenes.get(name)
    if old:
        for ob in list(old.objects):
            bpy.data.objects.remove(ob, do_unlink=True)
        bpy.data.scenes.remove(old)
    scene = bpy.data.scenes.new(name)
    bpy.context.window.scene = scene
    scene.render.engine = 'BLENDER_EEVEE'
    scene.eevee.taa_render_samples = 64
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    scene.render.image_settings.color_depth = '8'
    scene.render.use_file_extension = True
    scene.view_settings.view_transform = 'Standard'
    scene.view_settings.look = 'None'
    scene.view_settings.exposure = 0
    scene.view_settings.gamma = 1
    return scene

def camera(scene, at, target, scale):
    data = bpy.data.cameras.new('B12a_Camera')
    ob = bpy.data.objects.new('B12a_Camera', data)
    scene.collection.objects.link(ob)
    ob.location = at
    ob.rotation_euler = (Vector(target)-ob.location).to_track_quat('-Z','Y').to_euler()
    data.type = 'ORTHO'
    data.ortho_scale = scale
    scene.camera = ob
    return ob

def save_pixels(scene, pixels, path):
    height, width, _ = pixels.shape
    image = bpy.data.images.new('B12a_Output', width=width, height=height, alpha=True)
    image.pixels.foreach_set(np.ascontiguousarray(pixels, dtype=np.float32).ravel())
    image.filepath_raw = str(path)
    image.file_format = 'PNG'
    image.save()
    bpy.data.images.remove(image)
    clean_png(path)

def pixels_load(path):
    image = bpy.data.images.load(str(path), check_existing=False)
    w,h = image.size
    pixels = np.array(image.pixels[:], dtype=np.float32).reshape(h,w,4)
    bpy.data.images.remove(image)
    return pixels

def outline(pixels, radius, colour):
    alpha = pixels[:,:,3]
    h,w = alpha.shape
    expanded = np.zeros_like(alpha)
    for dy in range(-radius,radius+1):
        for dx in range(-radius,radius+1):
            if dx*dx+dy*dy > radius*radius:
                continue
            y0,y1 = max(0,dy), min(h,h+dy)
            x0,x1 = max(0,dx), min(w,w+dx)
            np.maximum(expanded[y0:y1,x0:x1], alpha[y0-dy:y1-dy,x0-dx:x1-dx], out=expanded[y0:y1,x0:x1])
    under = expanded * (1-alpha)
    out_alpha = alpha + under
    result = np.zeros_like(pixels)
    result[:,:,:3] = (pixels[:,:,:3]*alpha[:,:,None] + np.array(colour)*under[:,:,None]) / np.maximum(out_alpha[:,:,None],1e-8)
    result[:,:,3] = out_alpha
    return result

def render_lockups():
    scene = scene_new('B12a_TitleLockups')
    source_hashes = {}
    for name in ('SM_LF_Logo_LakeFight','SM_LF_Logo_DirectorsCut'):
        source = ROOT/'Export'/(name+'.fbx')
        source_hashes[name] = hashlib.sha256(source.read_bytes()).hexdigest()
        bpy.ops.import_scene.fbx(filepath=str(source))
    meshes = [ob for ob in scene.objects if ob.type=='MESH']
    assert len(meshes)==2
    # Originals share origin and already contain the complete title/ribbon layout.
    bpy.context.view_layer.update()
    coords = [ob.matrix_world @ Vector(v) for ob in meshes for v in ob.bound_box]
    lo = Vector(tuple(min(v[i] for v in coords) for i in range(3)))
    hi = Vector(tuple(max(v[i] for v in coords) for i in range(3)))
    centre = (lo+hi)*.5
    cam = camera(scene, (centre.x,-15,centre.z), centre, 6)
    for at, power, size in [((-3,-4,6),500,5),((4,-3,1),250,4)]:
        data = bpy.data.lights.new('B12a_Softbox','AREA')
        data.energy = power
        data.shape = 'DISK'
        data.size = size
        ob = bpy.data.objects.new('B12a_Softbox',data)
        scene.collection.objects.link(ob)
        ob.location = at
        ob.rotation_euler = (centre-ob.location).to_track_quat('-Z','Y').to_euler()
    world = bpy.data.worlds.get('B12a_World') or bpy.data.worlds.new('B12a_World')
    scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get('Background')
    bg.inputs['Color'].default_value = (.72,.78,.86,1)
    bg.inputs['Strength'].default_value = .5
    results = []
    for label,(w,h,title,hero) in FORMATS.items():
        scene.render.resolution_x,scene.render.resolution_y = w,h
        x,y,bw,bh = title
        # Orthographic scale is horizontal at these landscape aspect ratios.
        units_per_pixel = max((hi.x-lo.x)/bw,(hi.z-lo.z)/bh)*1.035
        cam.data.ortho_scale = units_per_pixel*w
        cam.location.x = centre.x-(x+bw/2-w/2)*units_per_pixel
        cam.location.z = centre.z+(y+bh/2-h/2)*units_per_pixel
        cam.rotation_euler = (math.pi/2,0,0)
        base = OUT/('B12a_Base_'+label+'.png')
        scene.render.filepath = str(base)
        bpy.ops.render.render(write_still=True)
        pixels = pixels_load(base)
        hx,hy,hw,hh = hero
        assert pixels[h-hy-hh:h-hy,hx:hx+hw,3].max()==0, 'Hero area must remain transparent'
        radius = max(2,round(w/240))
        for variant,colour in [('Light',(.96,.90,.70)),('DarkOutline',(.018,.031,.035))]:
            path = OUT/('LF_TitleLockup_'+label+'_'+variant+'.png')
            save_pixels(scene,outline(pixels,radius,colour),path)
            results.append(validate(path))
        base.unlink()
    return source_hashes, results

def validate(path):
    raw = path.read_bytes()
    w,h,depth,colour = struct.unpack('>IIBB',raw[16:26])
    assert depth==8 and colour==6,(path.name,depth,colour)
    pixels = pixels_load(path)
    alpha = pixels[:,:,3]
    occupied = np.argwhere(alpha>.01)
    y0,x0 = occupied.min(axis=0)
    y1,x1 = occupied.max(axis=0)
    assert alpha.min()==0 and alpha.max()>.99
    assert x0>1 and y0>1 and x1<w-2 and y1<h-2
    return {'file':path.name,'size':[w,h],'rgba_8bit':True,'alpha_range':[float(alpha.min()),float(alpha.max())],
            'bounds_top_left_px':[int(x0),int(h-1-y1),int(x1),int(h-1-y0)],
            'sha256':hashlib.sha256(raw).hexdigest()}

def emission(name,colour):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    nodes.clear()
    out = nodes.new('ShaderNodeOutputMaterial')
    shader = nodes.new('ShaderNodeEmission')
    shader.inputs[0].default_value = (*colour,1)
    mat.node_tree.links.new(shader.outputs[0],out.inputs[0])
    return mat

def rect(scene,name,x,y,w,h,z,mat):
    me = bpy.data.meshes.new(name)
    me.from_pydata([(x,y,z),(x+w,y,z),(x+w,y+h,z),(x,y+h,z)],[],[(0,1,2,3)])
    ob = bpy.data.objects.new(name,me)
    scene.collection.objects.link(ob)
    me.materials.append(mat)

def text(scene,body,x,y,size,mat):
    data = bpy.data.curves.new('B12a_GuideLabel','FONT')
    data.body = body
    data.size = size
    ob = bpy.data.objects.new('B12a_GuideLabel',data)
    scene.collection.objects.link(ob)
    ob.location = (x,y,2)
    data.materials.append(mat)

def render_guides():
    results = []
    for label,(w,h,title,hero) in FORMATS.items():
        scene = scene_new('B12a_Layout_'+label)
        scene.render.resolution_x,scene.render.resolution_y = w,h
        scene.render.film_transparent = False
        camera(scene,(w/2,h/2,1000),(w/2,h/2,0),w)
        navy = emission('B12a_Guide_Navy',(.022,.055,.070))
        cream = emission('B12a_Guide_Cream',(.94,.90,.74))
        teal = emission('B12a_Guide_Teal',(.07,.28,.30))
        orange = emission('B12a_Guide_Orange',(.95,.38,.13))
        rect(scene,'B12a_Background',0,0,w,h,0,navy)
        x,y,bw,bh = hero
        rect(scene,'B12a_Hero',x,h-y-bh,bw,bh,.2,teal)
        # Hero focal/action safe box; background may cover the whole image.
        for xx,yy,ww,hh in [(x,h-y-bh,bw,2),(x,h-y-2,bw,2),(x,h-y-bh,2,bh),(x+bw-2,h-y-bh,2,bh)]:
            rect(scene,'B12a_HeroBorder',xx,yy,ww,hh,.3,orange)
        size = max(14,w/55)
        text(scene,'HERO SHOT / BOAT BRAWL',x+14,h-y-bh/2,size,cream)
        text(scene,'Keep faces + boat action in this area',x+14,h-y-bh/2-size*1.5,size*.64,cream)
        text(scene,'Background can fill the canvas',x+14,h-y-bh+14,size*.58,cream)
        path = OUT/('LF_KeyArt_Layout_'+label+'.png')
        scene.render.filepath = str(path)
        bpy.ops.render.render(write_still=True)
        clean_png(path)
        # Composite the actual overlay at exactly its delivered position.
        pixels = pixels_load(path)
        logo = pixels_load(OUT/('LF_TitleLockup_'+label+'_Light.png'))
        a = logo[:,:,3:4]
        pixels[:,:,:3] = logo[:,:,:3]*a+pixels[:,:,:3]*(1-a)
        save_pixels(scene,pixels,path)
        check = pixels_load(path)
        assert check.shape==(h,w,4) and check[:,:,3].min()==1
        results.append({'file':path.name,'size':[w,h],'hero_rect_top_left_px':list(hero),
                        'title_rect_top_left_px':list(title),'opaque':True,
                        'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
    return results

def build():
    sources,images = render_lockups()
    guides = render_guides()
    for name,digest in sources.items():
        assert hashlib.sha256((ROOT/'Export'/(name+'.fbx')).read_bytes()).hexdigest()==digest
    report = {'blender':bpy.app.version_string,'source_fbx_sha256':sources,'outputs':images,'guides':guides,
              'notes':'Original B05 meshes only. Light = cream silhouette outline; DarkOutline = deep blue/charcoal outline. Transparent overlays, not hero renders. Guides are opaque and must not ship as finished art.'}
    path = OUT/'B12a_verified.json'
    previous = json.loads(path.read_text()) if path.exists() else None
    report['repeat_png_hashes_match'] = previous is not None and [i['sha256'] for i in previous['outputs']]==[i['sha256'] for i in images]
    report['source_fbxs_unchanged'] = True
    report['hero_areas_transparent'] = True
    path.write_text(json.dumps(report,indent=2))
    print(json.dumps(report))
    return report

if __name__=='__main__':
    REPORT = build()
