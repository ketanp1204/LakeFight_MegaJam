"""B07 socket screenshot. Staging axes/captions never enter the paddle FBX."""
import bpy,sys,os,math,json
from mathutils import Vector
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf
import lf_b02 as b
import lf_b03 as b3
col=lf.fresh_collection("_B07_Review")
g=b.Geometry()
for name in ["SOCKET_Grip_R","SOCKET_Grip_L"]:
    socket=bpy.data.objects[name]
    origin=socket.matrix_world.translation
    for axis,material in [((1,0,0),"PaintRed"),((0,1,0),"PaintGreen"),((0,0,1),"PaintBlue")]:
        end=origin+socket.matrix_world.to_3x3()@Vector(axis)*.14
        g.cylinder(origin,end,.0025,material,sides=4,end_radius=.0015)
    data=bpy.data.curves.new("B07_Label","FONT")
    data.body=name;data.size=.035
    data.materials.append(lf.mat("PaintCream"))
    ob=bpy.data.objects.new("B07_Label",data);col.objects.link(ob)
    ob.location=(origin.x,-.25,.035)
g.beam((.035,-.14,.02),(.585,-.14,.02),.003,.003,"PaintCream")
for x in [.035,.585]:g.beam((x,-.16,.02),(x,-.12,.02),.003,.003,"PaintCream")
axes=b.make_object(g,"B07_SocketAxes",col)
paddle=bpy.data.objects["SM_LF_Paddle"]
names={o.name for o in col.objects}|{paddle.name,"SOCKET_Grip_R","SOCKET_Grip_L"}
for ob in bpy.context.scene.objects:ob.hide_set(ob.name not in names)
bpy.ops.object.select_all(action="DESELECT")
area=next(a for a in bpy.context.screen.areas if a.type=="VIEW_3D")
region=next(r for r in area.regions if r.type=="WINDOW")
space=area.spaces.active
b.enum_set(space.shading,"type","SOLID");b.enum_set(space.shading,"color_type","MATERIAL")
b.enum_set(space.shading,"background_type","VIEWPORT");space.shading.background_color=(.035,.045,.06)
space.overlay.show_overlays=True
space.overlay.show_floor=False;space.overlay.show_axis_x=False;space.overlay.show_axis_y=False
b.enum_set(space.region_3d,"view_perspective","ORTHO")
space.region_3d.view_location=(.80,-.04,.045);space.region_3d.view_distance=1.65
scene=bpy.context.scene
scene.render.resolution_x=1600;scene.render.resolution_y=800;scene.render.resolution_percentage=100
paths=[]
for direction,file in [((.2,-3,5),"B07_Paddle_Sockets"),((0,0,1),"B07_Paddle_Sockets_Top")]:
    space.region_3d.view_rotation=b3.view_quaternion(direction)
    scene.render.filepath=os.path.join(b.ROOT,"_shots",file+".png")
    with bpy.context.temp_override(area=area,region=region):bpy.ops.render.opengl(write_still=True,view_context=True)
    paths.append(scene.render.filepath)
print("B07 screenshots saved",paths)
REPORTS=dict(screenshots=paths,sockets_visible=True,axes="X red shaft-forward; Y green; Z blue blade-normal")
