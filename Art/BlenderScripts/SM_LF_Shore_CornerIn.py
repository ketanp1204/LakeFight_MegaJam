"""B03 concave shore quarter-turn, R4m waterline, land outside the radius."""
import sys, importlib
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_b03 as b3
importlib.reload(b3)
REPORTS=[b3.shore("CornerIn")]
