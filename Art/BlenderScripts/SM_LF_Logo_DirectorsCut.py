"""B05 separate red DIRECTOR'S CUT plank with original extruded Cream lettering."""
import sys,importlib
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf;importlib.reload(lf)
import lf_b05 as b5;importlib.reload(b5)
g=b5.b.Geometry()
centre=-.99
outline=[(-2.10,centre-.18),(-1.96,centre),(-2.10,centre+.21),
         (-1.48,centre+.22),(1.55,centre+.205),(2.10,centre+.18),
         (1.97,centre),(2.10,centre-.22),(-1.60,centre-.205)]
g.prism(outline,.12,"PaintRed",plane="XZ")
b5.lettering(g,"DIRECTOR'S CUT",.213,.275,.038,.13,centre,-.067,.065,202611130502)
REPORTS=[b5.finish(g,"SM_LF_Logo_DirectorsCut",5000,
    "Separate notched red wooden ribbon/plank with original Cream DIRECTOR'S CUT geometry. Shared origin with main sign; toggle independently without repositioning.",
    origin="same as SM_LF_Logo_LakeFight; plank centre baked at Z=-.99m",title="DIRECTOR'S CUT",
    lettering_source="explicit original GLYPHS cell masks in lf_b05.py",plank_centre_z_m=centre,
    main_logo="SM_LF_Logo_LakeFight")]
