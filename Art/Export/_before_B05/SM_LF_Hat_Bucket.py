"""B04 floppy hollow bucket hat; four crew colours, origin at assumed head pivot."""
import sys,importlib,math
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf;importlib.reload(lf)
import lf_b04 as b4;importlib.reload(b4)
REPORTS=[]
for team in b4.TEAMS:
    g=b4.b.Geometry()
    profile=[(.112,.098,.245,0),(.130,.111,.225,0),(.140,.122,.145,0),
             (.205,.175,.119,.012),(.203,.173,.107,.012),
             (.128,.109,.135,0),(.114,.097,.223,0)]
    g.loft([b4.ellipse_ring(rx,ry,z,wave=w) for rx,ry,z,w in profile],"Paint"+team)
    b4.belt(g,.139,.121,.151,.026,"PaintCream")
    # Small solid fisherman hook patch; no texture or external logo.
    g.box((.145,-.016,.187),(.012,.032,.027),"WoodDark",bevel=.003)
    REPORTS.append(b4.finish(g,"SM_LF_Hat_Bucket_"+team,"head",
        "Hollow 12-sided crown, 12mm wavy floppy brim, cream band and dark sewn patch. Four identical shapes with crew colour changes only.",
        "Brim opening centre +10.125cm Z from head pivot; outer crown +18.375cm Z; no separate UE translation assumed.",team))
