# Deviations from sources and from the TSD

| # | Where | Source says | We do | Reason |
| --- | --- | --- | --- | --- |
| D1 | Phase 0 deps | TSD §1 lists pocketfft and nlohmann/json | Not fetched yet | Unused until Phase 5 (thermal) and the CLI; fetched when first needed |
| D2 | Phase 0 CMake | TSD §1: CMake ≥ 3.24 | pybind11 v3.0.1 (≥ 2.11 satisfied) | Needed for Python 3.14 |
