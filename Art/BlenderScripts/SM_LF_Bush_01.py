"""B10 original SM_LF_Bush_01; run inside Blender 5.1 via MCP."""
import sys, importlib
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import lf_b10 as b
importlib.reload(b)
REPORT = b.build('SM_LF_Bush_01')
print(REPORT)
