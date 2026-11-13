"""B08 original ICON_LF_Crew_Green. Idempotent named collection, FBX and preview/PNG."""
import sys, importlib
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import lf_b08 as b
importlib.reload(b)
REPORT = b.build('ICON_LF_Crew_Green')
print(REPORT)
