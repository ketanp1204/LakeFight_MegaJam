"""B03 8m grassy/sandy shore module. Run inside Blender; owns its collection."""
import sys, importlib
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_b03 as b3
importlib.reload(b3)
REPORTS=[b3.shore("Straight")]
