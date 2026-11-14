"""N1 item 10: foliage sway - run INSIDE the Unreal editor (py console / -ExecutePythonScript). Idempotent.

M_LF_Moss (a palette material instance) is shared by trees, bushes, ground, shore and islands, so the wind cannot go on it.
This script duplicates Moss's parent as M_LF_FoliageSway, adds a height-weighted world-position-offset wind to the copy,
makes MI_LF_FoliageSway with Moss's parameter values, and puts it ONLY in the Moss slot of the tree / bush meshes.
"""
import unreal

MAT_DIR = "/Game/LakeFight/Materials"
SWAY = MAT_DIR + "/M_LF_FoliageSway"
SWAY_MI = MAT_DIR + "/MI_LF_FoliageSway"
MOSS = "/Game/LakeFight/Meshes/M_LF_Moss"
MESHES = ["SM_LF_Tree_Pine", "SM_LF_Tree_Round", "SM_LF_TreeCluster_01", "SM_LF_Bush_01", "SM_LF_Bush_02"]
AMOUNT_CM = 7.0     # sway at the top of a ~5 m tree
SPEED = 1.3         # rad/s
FULL_HEIGHT = 520.0 # cm (local Z) where the sway reaches AMOUNT_CM; zero at the base

EAL = unreal.EditorAssetLibrary
L = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log("[N1 sway] " + msg)


moss = EAL.load_asset(MOSS)
parent = moss.get_editor_property("parent")
log("Moss parent: %s" % parent.get_path_name())
for p in (SWAY_MI, SWAY):
    if EAL.does_asset_exist(p):
        EAL.delete_asset(p)
mat = EAL.duplicate_asset(parent.get_path_name().split(".")[0], SWAY)

# Wind: sin(time * SPEED + world XY phase) * AMOUNT * saturate(localZ / FULL_HEIGHT)^2 along (1, 0.6, 0).
t = L.create_material_expression(mat, unreal.MaterialExpressionTime, -1400, 900)
ts = L.create_material_expression(mat, unreal.MaterialExpressionMultiply, -1250, 900)
ts.set_editor_property("const_b", SPEED)
wp = L.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1400, 1000)
wpm = L.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1250, 1000)
wpm.set_editor_property("r", True)
wpm.set_editor_property("g", True)
wpm.set_editor_property("b", False)
k = L.create_material_expression(mat, unreal.MaterialExpressionConstant2Vector, -1250, 1100)
k.set_editor_property("r", 0.004)
k.set_editor_property("g", 0.003)
dot = L.create_material_expression(mat, unreal.MaterialExpressionDotProduct, -1100, 1000)
ph = L.create_material_expression(mat, unreal.MaterialExpressionAdd, -950, 950)
sn = L.create_material_expression(mat, unreal.MaterialExpressionSine, -800, 950)
sn.set_editor_property("period", 6.283185)
lp = L.create_material_expression(mat, unreal.MaterialExpressionLocalPosition, -1400, 1250)
lpm = L.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1250, 1250)
lpm.set_editor_property("r", False)
lpm.set_editor_property("g", False)
lpm.set_editor_property("b", True)
h = L.create_material_expression(mat, unreal.MaterialExpressionDivide, -1100, 1250)
h.set_editor_property("const_b", FULL_HEIGHT)
hs = L.create_material_expression(mat, unreal.MaterialExpressionSaturate, -950, 1250)
h2 = L.create_material_expression(mat, unreal.MaterialExpressionMultiply, -800, 1250)
amt = L.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -800, 1100)
amt.set_editor_property("parameter_name", "SwayAmount")
amt.set_editor_property("default_value", AMOUNT_CM)
a1 = L.create_material_expression(mat, unreal.MaterialExpressionMultiply, -650, 1000)
a2 = L.create_material_expression(mat, unreal.MaterialExpressionMultiply, -500, 1100)
dirv = L.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -500, 1250)
dirv.set_editor_property("constant", unreal.LinearColor(1.0, 0.6, 0.0, 0.0))
out = L.create_material_expression(mat, unreal.MaterialExpressionMultiply, -350, 1100)
L.connect_material_expressions(t, "", ts, "A")
L.connect_material_expressions(wp, "", wpm, "")
L.connect_material_expressions(wpm, "", dot, "A")
L.connect_material_expressions(k, "", dot, "B")
L.connect_material_expressions(ts, "", ph, "A")
L.connect_material_expressions(dot, "", ph, "B")
L.connect_material_expressions(ph, "", sn, "")
L.connect_material_expressions(lp, "", lpm, "")
L.connect_material_expressions(lpm, "", h, "A")
L.connect_material_expressions(h, "", hs, "")
L.connect_material_expressions(hs, "", h2, "A")
L.connect_material_expressions(hs, "", h2, "B")
L.connect_material_expressions(sn, "", a1, "A")
L.connect_material_expressions(amt, "", a1, "B")
L.connect_material_expressions(a1, "", a2, "A")
L.connect_material_expressions(h2, "", a2, "B")
L.connect_material_expressions(a2, "", out, "A")
L.connect_material_expressions(dirv, "", out, "B")
L.connect_material_property(out, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
L.recompile_material(mat)
EAL.save_loaded_asset(mat, False)

# Instance with Moss's look.
mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset("MI_LF_FoliageSway", MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
L.set_material_instance_parent(mi, mat)
for name in L.get_vector_parameter_names(moss):
    L.set_material_instance_vector_parameter_value(mi, name, L.get_material_instance_vector_parameter_value(moss, name))
for name in L.get_scalar_parameter_names(moss):
    L.set_material_instance_scalar_parameter_value(mi, name, L.get_material_instance_scalar_parameter_value(moss, name))
L.update_material_instance(mi)
EAL.save_loaded_asset(mi, False)
log("copied %d vector / %d scalar params from Moss" % (len(L.get_vector_parameter_names(moss)), len(L.get_scalar_parameter_names(moss))))

for name in MESHES:
    sm = EAL.load_asset("/Game/LakeFight/Meshes/" + name)
    n = 0
    for i, slot in enumerate(sm.static_materials):
        mi_slot = slot.material_interface
        if mi_slot and mi_slot.get_name() in ("M_LF_Moss", "MI_LF_FoliageSway"):
            sm.set_material(i, mi)
            n += 1
    EAL.save_loaded_asset(sm, False)
    log("%s: %d slot(s) -> MI_LF_FoliageSway" % (name, n))
