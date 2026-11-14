"""B10.1 SM_LF_GroundPatch_02: preserve +Z top-face winding; run inside Blender 5.1 via MCP."""
import sys, importlib
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import lf_b10 as b
importlib.reload(b)
REPORT = b.build('SM_LF_GroundPatch_02')
# Keep the normal contract visible in the asset's generated report.
ob = b.bpy.data.objects['SM_LF_GroundPatch_02']
positive = sum(p.normal.z > 0 for p in ob.data.polygons)
assert positive == len(ob.data.polygons)
REPORT['top_faces_positive_z_percent'] = 100.0 * positive / len(ob.data.polygons)
(b.OUT / '_reports' / 'SM_LF_GroundPatch_02_B10.json').write_text(b.json.dumps(REPORT, indent=2))
print(REPORT)
