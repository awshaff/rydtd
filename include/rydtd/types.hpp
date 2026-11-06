// OWNER: agent
#pragma once
#include <Eigen/Dense>
#include <complex>

namespace rtd {

/// Complex double, the only scalar type in the engine (CLAUDE.md: doubles only).
using cplx = std::complex<double>;

/// Fixed-size complex NxN matrix; used for density matrices and Hamiltonians
/// (N <= 6, so it lives on the stack: no heap allocation in the hot loop).
/// Layout: column-major (Eigen default), see TSD section 2.
template <int N>
using Mat = Eigen::Matrix<cplx, N, N>;

}  // namespace rtd
