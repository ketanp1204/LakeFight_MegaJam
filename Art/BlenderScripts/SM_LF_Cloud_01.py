"""B11 original SM_LF_Cloud_01. Run inside Blender 5.1 via MCP."""
import sys, importlib
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import lf_b11 as b
importlib.reload(b)
REPORT = b.build('SM_LF_Cloud_01')
print(REPORT)
