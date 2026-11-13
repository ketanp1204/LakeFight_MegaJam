"""B08 original SM_LF_FishSpot. Idempotent named collection, FBX and preview/PNG."""
import sys, importlib
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import lf_b08 as b
importlib.reload(b)
REPORT = b.build('SM_LF_FishSpot')
print(REPORT)
