"""B10 original ICON_LF_Paddle; run inside Blender 5.1 via MCP."""
import sys, importlib
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import lf_b10 as b
importlib.reload(b)
REPORT = b.build('ICON_LF_Paddle')
import B10_Review as review
importlib.reload(review)
REPORT['png_sha256'] = review.icon('ICON_LF_Paddle')
print(REPORT)
