"""rydtd: time-domain engine for Rydberg atomic superheterodyne receivers."""
from ._rydtd import has_openmp, platform_tag, version

__all__ = ["has_openmp", "platform_tag", "version"]
