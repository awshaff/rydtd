import sys
from pathlib import Path

# In-tree import: the compiled module is built into python/rydtd/ by CMake.
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "python"))
