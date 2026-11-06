// OWNER: agent
#include <pybind11/pybind11.h>

#include <string>

#include "rydtd/version.hpp"

namespace py = pybind11;

PYBIND11_MODULE(_rydtd, m) {
  m.doc() = "rydtd: C++ time-domain engine (compiled part)";
  m.def(
      "version", [] { return std::string(rtd::version()); },
      "Engine version string, e.g. '0.1.0'.");
  m.def("platform_tag", &rtd::platform_tag,
        "Build platform tag, e.g. 'linux-x86_64-gcc15' (cache key, TSD 1.3).");
  m.def("has_openmp", [] { return rtd::has_openmp(); }, "True if built with OpenMP.");
}
