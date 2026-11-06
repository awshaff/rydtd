# TSD — `rydtd` technical specification

**Version:** 0.2 · **Date:** 2026-11-06 · **Implements:** `FSD.md` v0.1
This document is the source of truth for equations, conventions, interfaces and validation targets. When it disagrees with a paper, it records the choice and the reason. Where it is silent, `rydsim/TSD.md` applies.

**Changes in 0.2:** two supported platforms (Linux x86-64, macOS arm64); OpenMP optional with a serial fallback; `-ffp-contract=off`; own normal-variate generator instead of `std::normal_distribution`; new target W24 (cross-platform agreement); pitfalls 13–14; profiling on macOS.

---

## 1. Toolchain and build

- **C++:** C++20, GCC ≥ 11 or Clang ≥ 14 (AppleClang from current Xcode on macOS), CMake ≥ 3.24 with `CMakePresets.json` (`debug`, `release`, `asan`).
- **Supported platforms:**

  | Platform | Compilers | Role |
  | --- | --- | --- |
  | Linux x86-64 (Aan's Intel PC, CI) | GCC, Clang | Long runs, figures, the official benchmark (T2) |
  | macOS arm64 (Aan's M2) | AppleClang + Homebrew `libomp`, or Homebrew LLVM | Daily development, debug builds, tests, sanitizers |

  On macOS the toolchain must be native arm64 (Homebrew under `/opt/homebrew`, not `/usr/local`). Under Rosetta, CMake reports `x86_64` and the architecture checks below pick the wrong branch.
- **C++ dependencies** (fetched with CMake `FetchContent`, versions pinned):
  - Eigen 3.4 (fixed-size complex matrices);
  - OpenMP (parallel velocity classes and realizations) — **optional**, see below;
  - Catch2 v3 (C++ tests);
  - pybind11 ≥ 2.11 (Python module);
  - pocketfft, header-only, BSD (thermal-field synthesis);
  - nlohmann/json (CLI config only).
- **Python:** ≥ 3.11, `rydsim`'s pinned stack, plus scikit-build-core (packaging), numba (benchmark only), pytest.

### 1.1 Compiler flags

| Flag | Where | Why |
| --- | --- | --- |
| `-O3 -DNDEBUG` | Release, all platforms | Optimization |
| `-march=x86-64-v2` | Release, **only** when `CMAKE_SYSTEM_PROCESSOR` matches `x86_64\|AMD64` | Portable x86 baseline. No `-march` on arm64: the compiler default is already a safe baseline. Never `-march=native` in published builds |
| `-ffp-contract=off` | All configurations, all platforms | Forbids fused multiply–add contraction. Clang contracts by default and arm64 has FMA, so without this flag the M2 and the Intel PC round differently (§8 pitfall 13) |
| `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` (+ `-Werror` in CI) | Our targets only, via the `INTERFACE` target `rydtd_warnings` | Strictness for our code without breaking on third-party warnings |
| `-fsanitize=address,undefined -fno-omit-frame-pointer` | `asan` preset | Memory and UB checks |

**Never** `-ffast-math` or `-Ofast`: they break IEEE semantics, NaN checks and determinism. Configure fails if either appears in `CMAKE_CXX_FLAGS`.

### 1.2 OpenMP is optional

- CMake option `RYDTD_OPENMP` (default `ON`). `find_package(OpenMP COMPONENTS CXX)` **without** `REQUIRED`.
- If found: `rydtd_core` links `OpenMP::OpenMP_CXX` as `PUBLIC`.
- If not found, or the option is off: build **serially** and print a CMake warning. Results are identical to a 1-thread OpenMP run on the same machine; only speed differs.
- **Code rules:**
  - Every call to the OpenMP runtime API (`omp_get_thread_num`, `omp_get_max_threads`, …) is wrapped in `#ifdef _OPENMP`.
  - `#pragma omp` lines may stay unguarded. In a serial build, CMake adds `-Wno-unknown-pragmas` (GCC) or `-Wno-source-uses-openmp` (Clang/AppleClang) to `rydtd_warnings`, otherwise `-Werror` turns the ignored pragma into a build failure.
  - Tests that need threads (W21) are skipped, not failed, when `_OPENMP` is undefined, and the skip is visible in the test report.
- **macOS setup:** `brew install cmake ninja libomp`, then configure with `-DOpenMP_ROOT="$(brew --prefix libomp)"`. If FindOpenMP still fails with AppleClang, use Homebrew LLVM (`brew install llvm`, set `CMAKE_CXX_COMPILER` to its `clang++`). Record the working recipe in `docs/build_macos.md` once it works on the M2.

### 1.3 Entry points and caching

- **Entry points:**
  - `make build` configures and builds the release preset and the Python module;
  - `make test` runs C++ tests (`ctest`) and Python tests (`pytest`);
  - `make validate` runs all W-targets and writes `out/validation_report.md`;
  - `make figures` builds G1–G9 and T1–T3;
  - `make bench` runs the benchmark (G9, T2);
  - `make all` runs everything.
- **Caching:** `cache/<module>/<sha1(config + engine version + platform tag)>.npz`. The platform tag (e.g. `linux-x86_64-gcc13`, `macos-arm64-appleclang16`) keeps results from the two machines apart until W24 has passed.
- **Mixing machines:** results that appear in the same figure or table come from one platform, unless W24 passes for that configuration.

## 2. Conventions

All `rydsim` TSD §2 conventions apply unchanged: SI units inside code, peak field amplitudes referred to the atoms, single-sided PSDs, T0 = 290 K. Engine-specific additions:

| Item | Convention |
| --- | --- |
| Time and frequency | Seconds and rad/s inside the engine. Configs may give f/2π in Hz; conversion happens once, in the config loader |
| Rabi frequency | Ω = μE/ħ with E the **peak** amplitude. Configs state whether a number is Ω or Ω/2π; a bare "6 MHz" from a paper must be resolved and recorded in `docs/deviations.md` |
| Field envelope | Real field component E(t) = Re[Ẽ(t) e^{iω_LO t}]. A tone of peak amplitude A at ω_LO + δ has Ẽ(t) = A e^{iδt}. δ = ω − ω_LO |
| Envelope PSD | Ẽ has a **two-sided** PSD S_env(Δf). Identity (§3.7): S_env(Δf) = S_E,th^peak(f_LO + Δf), the `rydsim` single-sided peak-convention PSD |
| Density matrix | ρ is an `Eigen::Matrix<std::complex<double>, N, N>`. Where a vectorized form is needed (Liouvillian), use column-major `vec(ρ)`, Eigen's default layout |
| Frame | Rotating frame in which probe, coupling and LO are static. Signals, interferers and the thermal field enter as complex envelopes on their RF edge |
| Sign of detuning | Δ = ω_laser − ω_atom. State the Hamiltonian sign convention in `hamiltonian.hpp` and in `docs/deviations.md` when it differs from a source |
| Doppler | Counter-propagating probe and coupling: Δp' = Δp − k_p v, Δc' = Δc + k_c v (Wu 2023 Eq. 15). RF Doppler off by default (§3.4) |
| Output | P(t) in W, on a uniform output grid with spacing `dt_out`; ρ₂₁^D(t) also stored |

Every public C++ function documents units and conventions in its doc comment.

## 3. Models and equations

### 3.1 Hamiltonian (FR-1)

- Levels 0 … N−1 (code) ↔ |1⟩ … |N⟩ (papers). The config gives a list of edges (a, b), each with a field label, a static Rabi frequency, a detuning, and, for optical edges, a wavevector and propagation sign.
- RWA, rotating frame. For the four-level superhet ladder this reproduces Zhu & Dai Eq. (1) and the matrix in Bridging-gaps (2026) Eq. (1) (up to their overall sign convention, which must be recorded).
- The RF edge carries Ω̃_RF(t) = Ω_LO + Σ_tones Ω_j e^{iδ_j t} + Ω̃_env(t) + Ω̃_th(t). Static and time-dependent parts are stored separately so the hot loop only evaluates the time-dependent part.
- Star-shaped graphs (one Rydberg level coupled to several RF levels, as in Cui et al.) use the same code path.

### 3.2 Dissipator (FR-1)

Lindblad form with:
- spontaneous decay channels γ_{ij} (population i → j), from config;
- pure dephasing per coherence (optional, e.g. laser linewidth);
- transit rate γ_t added to the decay of every density-matrix element, with repopulation γ_in into the ground state (Bridging-gaps 2026 Eq. 3 structure; Zhu & Dai Eqs. 6–7).

Implementation in matrix form, dρ/dt = −i[H, ρ] + D(ρ), for the time-domain engine. The vectorized Liouvillian L (N² × N²) is built only for the steady state and the response kernels.

### 3.3 Steady state and response kernels (FR-2)

- **Steady state:** solve L vec(ρ) = 0 with the trace constraint replacing one row. Eigen `PartialPivLU` on the N² × N² system (16 × 16 for N = 4). Warn if the estimated condition number exceeds 1e12.
- **Linear response:** with L = L₀ + Ω̃(t)·L₁ + Ω̃*(t)·L₁' around the static operating point,
  ```
  ρ^(1)(ω) = (iω I − L₀)^{-1} L₁ ρ₀
  H₁(iω) = ⟨readout, ρ^(1)(ω)⟩       (readout = Im ρ₂₁ → α → P)
  ```
- **Higher orders:** recursive perturbation, (i Σω − L₀) ρ^(n) = L₁ ρ^(n−1) (plus conjugate terms). H₃ and Ω_IIP3 = sqrt( (4/3)·|H₁/H₃| ) follow Chen/Yuen/See Eqs. (54)–(57).
- Doppler: average the kernels over the velocity grid of §3.4 (same grid as the time domain).
- Use `solve`, never explicit inverses.

### 3.4 Doppler velocity grid (FR-3) — read §8 pitfall 1 first

1-D Maxwell distribution along the beams, p(v) = exp(−v²/2σ_v²)/(√(2π) σ_v), σ_v = sqrt(k_B T/m).

**Default grid (composite):**
- **Core:** |v − v_c| ≤ v_core, uniform, N_core points, composite Simpson weights.
- **Wings:** v_core < |v − v_c| ≤ v_cut, Gauss–Legendre with N_w nodes per side.
- Weights multiply p(v); the total weight is checked against the normal CDF on [−v_cut, v_cut] to 1e-12.
- **Core centre v_c:** the core must contain every narrow feature. With laser detunings, the one-photon resonance sits at v = Δp/k_p and the two-photon resonance at v* = −(Δp + Δc)/(k_c − k_p). The loader computes both, widens or splits the core so both lie inside, and asserts it.
- **Starting values** (from this work's check, §6 W7a): v_core = 0.5σ_v, N_core = 801, N_w = 16, v_cut = 6σ_v. These are starting values, not defaults to trust. Every production config is re-converged (§7).

**Gauss–Hermite quadrature is not allowed** for any integrand containing EIT or Lorentzian features narrower than about 0.1σ_v (§8 pitfall 1).

**Wing treatment:** `wing_mode ∈ {"dynamic", "quasistatic"}`, default `"dynamic"`. In `"quasistatic"`, wing classes follow their instantaneous steady state (they are far detuned and respond much faster than the IF). Allowed only when the convergence test shows a QoI change below threshold.

**RF Doppler:** `rf_doppler: false` by default. Justification: σ_v·k_RF/2π ≈ 3.2 kHz at 6.9458 GHz and 300 K. LO and signal come from the same direction, so the IF is unchanged, and a kHz shift of the LO detuning is small against a MHz AT splitting. A test must show the QoI changes by less than 1e-3 when it is switched on.

**Reference numbers** (Cs, m = 132.905451961 u):

| T | σ_v | k_p σ_v/2π (852.35 nm) | (k_c − k_p) σ_v/2π (λ_c = 510 nm) |
| --- | --- | --- | --- |
| 290 K | 134.69 m/s | 158.0 MHz | 106.1 MHz |
| 300 K | 137.0 m/s | 160.7 MHz | 107.9 MHz |

### 3.5 Time-domain integration (FR-3)

- **Integrators:** templated on the state type.
  - `rk4<N>`: fixed step. Stability on the imaginary axis needs dt·|λ_max| ≲ 2.8, where λ_max is the largest Liouvillian eigenvalue magnitude of that velocity class (computed once per class from L₀ plus the largest expected drive). The engine sets dt per class from `dt_safety · 2.8/|λ_max|`, with `dt_safety` a named parameter (default 0.5).
  - `dopri5<N>`: adaptive Dormand–Prince 5(4) with dense output, named `rtol`, `atol`. Each class adapts its own step.
- **Initial state:** steady state of the class under static drives only.
- **Warm-up:** `t_warm` (named; start at 20 µs; Zhu & Dai rise time is 2.45 µs). Results before t_warm are discarded. Convergence test: doubling t_warm changes the QoI by less than threshold (§7).
- **Parallelism:** OpenMP over velocity classes inside one realization (serial when built without OpenMP, §1.2). The Doppler sum is a fixed-order reduction (each class writes its own slot; the sum runs serially in index order) so results do not depend on thread count.
- **Time-dependent drives:** sampled envelopes are interpolated (cubic Hermite by default; `interp` is a named parameter). Tones are evaluated analytically.
- **Health checks** every `check_every` steps: |Tr ρ − 1| < 1e-10 and ‖ρ − ρ†‖_max < 1e-10, else abort with the class index, time and values.

### 3.6 Readout (FR-6)

As `rydsim` TSD §3.5 (Zhu & Dai):
```
α(t) = −(k_p N0 μ12² / (ε0 ħ Ω_p)) · Im ρ₂₁^D(t)
P(t) = P0 · exp(−2 α(t) L)
```
- Default: optically thin slab, Ω_p constant along the cell (named approximation `probe_model: "slab"`).
- Optional later: `probe_model: "slices"` with n_x slices and probe depletion.

### 3.7 Thermal field (FR-5)

**Envelope PSD identity.** For a narrowband real process E(t) = Re[Ẽ(t) e^{iω₀t}] with circular complex envelope, ⟨E²⟩ = ⟨|Ẽ|²⟩/2. The single-sided rms-convention PSD of E integrates to ⟨E²⟩; the two-sided PSD of Ẽ integrates to ⟨|Ẽ|²⟩. Hence
```
S_env(Δf) = 2 · S_rms,ss(f_LO + Δf) = S_peak,ss(f_LO + Δf) = S_E,th(f_LO + Δf, T)   (rydsim §3.1)
```
for |Δf| ≤ B_th/2, and zero outside. Both the signal side (Δf > 0) and the image side (Δf < 0) of the LO are included automatically. Record the derivation in `docs/conventions_envelope.md` before Phase 5; W20b checks it numerically.

**Coherence factor:** `zeta` (default 1, which matches rydsim V3) scales S_env to account for the finite cell (`rydsim` §3.1). Record the value used per run.

**Rabi units:** S_Ω(Δf) = ζ·(μ_RF/ħ)²·S_env(Δf).

**Synthesis (FFT, per realization):**
```
T_rec = N_s · Δt_n,  Δf_k = k / T_rec  (k in the FFT ordering, both signs)
c_k = sqrt( S_Ω(Δf_k) / T_rec ) · g_k,   g_k ~ CN(0, 1)  (E|g_k|² = 1)
Ω̃_th(t_n) = Σ_k c_k exp(i 2π Δf_k t_n)
```
- Check: ⟨|Ω̃_th|²⟩ = Σ_k S_Ω(Δf_k)/T_rec ≈ ∫ S_Ω df.
- Oversampling: Δt_n ≤ 1/(10·B_th) (named `noise_oversample`, default 10); interpolate as in §3.5.
- The record is periodic: T_rec ≥ t_warm + t_run, and wrap-around is never used.
- **One realization of the field is shared by all velocity classes** (all atoms see the same field).
- Classical field: the vacuum term is omitted. At 6.9458 GHz and 290 K, hf/(k T) ≈ 1.15e-3 (rydsim V20), so the error is about 0.1%. Named flag `include_vacuum: false`.
- Because the field is band-limited and smooth on the integrator step, each realization is an ordinary ODE; no Itô/Stratonovich calculus is needed. The loader asserts B_th ≤ 0.1/dt_max.

**Seeds and random variates:**
- `seed_r = splitmix64(master_seed ^ r)` for realization r, feeding `std::mt19937_64`. Seeds depend on the realization index, never on the thread id.
- `std::mt19937_64` is fully specified by the C++ standard, so its raw 64-bit output is the same on every platform. The **distributions are not**: `std::normal_distribution` and `std::uniform_real_distribution` produce different numbers in libstdc++ (Linux GCC) and libc++ (macOS) from the same engine state (§8 pitfall 14). Therefore:
  - uniform doubles: `u = ((x >> 11) + 0.5) · 2⁻⁵³` from the raw output `x` (53 random bits, never exactly 0 or 1);
  - normal variates: own Box–Muller on two such uniforms, in `rng.hpp`;
  - `std::*_distribution` is not used anywhere in the engine.

### 3.8 Electronic readout noise (Python)

The engine returns P(t) without detector noise. The analysis layer converts P(t) to photocurrent and adds photon shot noise, RIN, TIA and Johnson noise with `rydsim.atomic.noise`, using the same seeds scheme. This is valid because these sources are additive after detection. RIN acting back on the atoms through Ω_p is neglected (§8 pitfall 10).

### 3.9 Metrics (FR-7)

- **Coherent sampling:** tone offsets and run length are chosen so every tone has an integer number of cycles in the analysis window. Tone amplitudes are read from FFT bins with a rectangular window. Noise PSDs use Welch with a Hann window; the ENBW is stated.
- **IF demodulation:** complex mixing at δ_s, then a low-pass filter with stated bandwidth.
- **P1dB:** Wu et al. definition. Ω_1dB is the signal Rabi frequency at which the IF tone power is 1 dB below the linear extrapolation |κ|_max·Ω_s.
- **IMD3 / IIP3:** two equal tones at δ₁, δ₂. IMD3 (dBc) = 20·log10|A(2δ₁ − δ₂)/A(δ₁)| at the IF output. IIP3 by extrapolating fitted slope-1 and slope-3 lines, using only points where the local slopes are within ±0.1 of 1 and 3.
- **Desensitization (RQ-A):** interferer at δ_int plus a small test tone at δ_s with Ω_s ≤ Ω_1dB/100, plus the thermal field.
  ```
  κ_eff   = A_IF(δ_s) / Ω_s                    (with the interferer present)
  S_out   = IF noise PSD near δ_s (tone bins excluded)
  NEF_in  = sqrt(S_out) / |κ_eff|, converted to field units with ħ/μ_RF
  ΔNEF    = 20·log10( NEF_in(with interferer) / NEF_in(without) )   [dB]
  ```
  Report ΔNEF for thermal-only noise and for total noise (thermal + electronic). **Blocking point:** interferer field at which ΔNEF = 3 dB.
- **Wu's anti-blocking metric (noiseless):** Ω_int3 is the interferer Rabi frequency at which the IF signal amplitude falls by a factor √2 relative to no interferer (Wu 2023 Sec. IV, reproduced literally).
- **Noise-conversion ratio (RQ-B):** with internal noise off,
  ```
  R_conv = S_IF,measured(band around δ_s) / S_IF,linear
  S_IF,linear = Σ_{sidebands} |H₁(iω)|² · S_Ω(Δf)   (signal and image sidebands, same ζ)
  ```
  R_conv = 1 in the linear regime; this is W19 when no strong tone is present.
- **EVM (RQ-D):** rms error vector divided by rms reference constellation, after one-tap least-squares gain and phase correction. QAM order, symbol rate and roll-off are config inputs (FSD Q5).

## 4. Interfaces

```cpp
namespace rtd {

template <int N>
using Mat = Eigen::Matrix<std::complex<double>, N, N>;

struct Edge {                 // a < b, level indices
  int a, b;
  std::string field;          // "probe", "coupling", "rf0", ...
  double omega_static;        // rad/s, peak convention
  double detuning;            // rad/s
  double k_wave;              // 1/m, 0 for RF
  int    dir;                 // +1 or -1 along the beam axis
};

template <int N>
struct Model { std::array<Edge, /*max*/ 2 * N> edges; /* decays, transit, repop */ };

struct VelocityGrid { std::vector<double> v, w; };   // w includes p(v)

struct Envelope {             // sampled complex envelope on one RF edge
  double t0, dt;
  std::vector<std::complex<double>> samples;         // rad/s
};

struct RunConfig {
  double t_warm, t_run, dt_out;
  enum class Integrator { RK4, DOPRI5 } integrator;
  double dt_safety, rtol, atol;
  int check_every;
};

struct Result {
  std::vector<double> t;                             // s
  std::vector<std::complex<double>> rho21;           // Doppler-averaged
  std::vector<double> P;                             // W
  /* metadata: steps per class, rejected steps, wall time, platform tag */
};

template <int N>
Result run(const Model<N>&, const VelocityGrid&,
           const std::vector<Envelope>&, const RunConfig&,
           std::uint64_t master_seed, int realization);

std::string platform_tag();   // e.g. "macos-arm64-appleclang16", from compile-time macros

}  // namespace rtd
```

- Python: `rydtd.run(cfg: dict, master_seed: int, realization: int) -> dict`. It dispatches on N at runtime to the compiled instances N = 3, 4, 5, 6.
- Configs are YAML, parsed in Python. The CLI (`apps/rydtd_cli`) reads the same content as JSON, for benchmarks and profiling without Python.
- No `virtual` calls and no `std::function` inside the per-step path (AGENTS.md §2).

## 5. Repository layout

```
rydtd/
  CMakeLists.txt, CMakePresets.json, pyproject.toml   # scikit-build-core
  include/rydtd/
    types.hpp          # Mat<N>, constants, units helpers
    model.hpp          # Edge, Model, config validation
    hamiltonian.hpp    # §3.1
    lindblad.hpp       # §3.2 matrix-form RHS
    liouvillian.hpp    # vectorized L for §3.3
    steady_state.hpp   # §3.3
    response.hpp       # H1, H3, IIP3
    doppler.hpp        # §3.4 composite grid
    integrators.hpp    # rk4, dopri5
    drives.hpp         # tones, envelopes, interpolation
    thermal.hpp        # §3.7 synthesis
    rng.hpp            # splitmix64, seeding, uniform and Box–Muller (§3.7)
    readout.hpp        # §3.6
    run.hpp            # orchestration
  src/                 # non-template code
  bindings/py_rydtd.cpp
  apps/rydtd_cli.cpp
  tests/cpp/           # Catch2: unit tests, W1–W8, W20–W22, W24
  python/rydtd/        # configs.py, analysis.py (§3.9), validation.py, readout_noise.py (calls rydsim)
  tests/py/            # pytest: W9–W19, W23
  bench/               # numpy_ref.py, numba_ref.py, run_bench.py
  configs/             # YAML per validation case and figure; every number sourced
  figures/             # fig_G1.py … fig_G9.py, tab_T1.py … tab_T3.py
  docs/                # deviations.md, conventions_envelope.md, build_macos.md, learning/phase<k>.md
  refs/                # source papers, read-only
  out/, cache/         # generated, git-ignored
```

## 6. Validation targets

Prefix **W** avoids clashing with `rydsim`'s V-numbers. Tolerances are relative unless marked. **Blocking** targets gate a release. **Targets may not be edited to make a test pass.**

### 6.1 Identities and numerics

| # | Quantity | Target | Tol. | Blocking | Source |
| --- | --- | --- | --- | --- | --- |
| W1 | Two-level steady state, ρ_ee = (Ω²/4)/(Δ² + Γ²/4 + Ω²/2), over a grid of (Ω, Δ) | analytic | 1e-10 | yes | identity |
| W2 | Trace and Hermiticity after 1e6 RK4 steps (four-level, Jing parameters, one class, γ_in = γ_t so the trace is conserved) | \|Tr ρ − 1\|, ‖ρ − ρ†‖ < 1e-12 | — | yes | identity |
| W3 | Long time-domain run → steady-state solver | agreement | 1e-8 abs | yes | consistency |
| W4 | RK4 order: error vs dt on a driven three-level problem; DOPRI5 error ∝ rtol | slope 4.0 / 1.0 | ±0.2 | yes | numerics |
| W5 | Jing: E = 3.0 mV/cm rms, μ_RF = 1443.450 e·a0 → Ω/2π | 7.836 MHz (Jing quote 7.9) | 0.1% (and ≤ 1% of 7.9) | yes | Jing 2020 Fig. 2 / Methods |
| W6 | σ_v and Doppler widths of §3.4 table | table values | 0.1% | yes | identity |
| W7a | Doppler grid on the weak-probe three-level test integrand (below): composite starting grid vs 8e6-point reference | rel. error < 2e-3 | — | yes | this work |
| W7b | Each production config: QoI change when N_core ×2, N_w ×2, v_cut + 2σ_v | < 1e-3 | — | yes | convergence |
| W8 | H₁(iω) from §3.3 vs time-domain tone with Ω_s/Ω_LO = 1e-3, at 10 kHz, 100 kHz, 1 MHz (Zhu & Dai parameters) | ratio 1, phase 0 | 1%, 1° abs | yes | consistency |

**W7a test integrand** (this work): three-level weak-probe ladder with resonant lasers, Cs at 300 K, λ_p = 852.35 nm, λ_c = 510 nm, γ₂ = 2π·5.2 MHz, γ₃ = 2π·10 kHz, Ω_c = 2π·0.97 MHz,
```
ρ₂₁(v) = i / [ γ₂/2 − iΔp' + (Ω_c²/4) / (γ₃/2 − i(Δp' + Δc')) ]
```
Measured in this work: Gauss–Hermite with 64 nodes gives an 87% error on ⟨ρ₂₁⟩ (128 nodes: 81%; 256 nodes: 73%). The composite grid (801 core points over ±0.5σ_v, 16 Gauss–Legendre nodes per wing to 6σ_v) gives 0.15%. γ₃ here is a test value, not a physical claim.

### 6.2 Published anchors

| # | Quantity | Target | Tol. | Blocking | Source |
| --- | --- | --- | --- | --- | --- |
| W9 | DC intrinsic gain κ(i0) | −8.67e-13 W/Hz | 10% | yes | Zhu & Dai §III-E (= rydsim V23) |
| W10 | DC quantum transconductance g_q(i0) | −1.4 mS | 10% | yes | Zhu & Dai §V-B (= rydsim V24) |
| W11 | Step-response rise time / 3-dB bandwidth of g_q, T = 0 K | 2.45 µs / 0.14 MHz | 15% | yes | Zhu & Dai §V-B |
| W12 | LO field maximizing the IF response \|P(δ_s)\| (Jing parameters, Doppler-averaged; Jing's E₁ in Fig. 2a — confirm it is the LO field) | 3.0 mV/cm rms (4.24 mV/cm peak) | 15% | yes | Jing 2020 Fig. 2a |
| W13 | IF amplitude vs E_s: log–log slope over ≥ 60 dB below Ω_1dB (Jing parameters) | 1.00 | ±0.01 abs | yes | Jing 2020 Fig. 2c (linear regime) |
| W14 | Linearized f_3dB: EIT four-level / SWM six-level | 0.66 / 7.2 MHz | 15% | no (blocked by FSD Q2) | Chen/Yuen/See Sec. IV |
| W15 | Volterra-3 vs time-domain IMD3, for Ω_s ≤ Ω_1dB/10 and IMD3 above the numerical floor | agreement | 0.5 dB abs | yes | this work |
| W16 | Wu trends: (a) Ω_1dB rises with Ω_p, cold and thermal; (b) thermal Ω_1dB > cold at equal Ω_p; (c) Ω_int3 rises with Ω_p; (d) thermal Ω_int3 > cold | all four hold over the swept Ω_p range | shape | yes | Wu 2023 Figs. 5, 7 |
| W17 | Wu interference spectrum: products at Δf₂, 2Δf₂, Δf₂ ± Δf₁ more than 40 dB above the numerical floor, Δf₂ the largest | holds | shape | yes | Wu 2023 Fig. 6(a) |
| W18 | Cui multi-band, N = 1, 2, 4, 8: NMSE between engine and Cui's linear model; small for N ≤ 4, larger at N = 8 | values from Cui Fig. 8 | to set after digitizing | no (FSD Q4) | Cui TWC 2026 Sec. VI-B |
| W23 | Superhet saturation field in the optically-biased setup | 1.84 mV/cm | 20% | no (FSD Q3) | Optically-biased receiver 2025, Fig. 4 |

### 6.3 Stochastic field and engine properties

| # | Quantity | Target | Tol. | Blocking | Source |
| --- | --- | --- | --- | --- | --- |
| W19 | IF noise PSD from ≥ 20 realizations vs Σ \|H₁\|²·S_Ω (Zhu & Dai parameters, B_th = 2 MHz, no strong tone) | ratio 1 | 10% | yes | rydsim V32 |
| W20a | Synthesized envelope PSD vs target, averaged over 100 realizations, in-band bins | ratio 1 | 5% | yes | §3.7 |
| W20b | Envelope identity: mean \|Ω̃_th\|² vs ζ(μ/ħ)²∫S_E,th(f_LO + Δf)dΔf | ratio 1 | 2% | yes | §3.7 |
| W20c | Gaussianity: excess kurtosis of Re Ω̃_th | 0 | 0.05 abs | yes | §3.7 |
| W20d | Out-of-band leakage | < −60 dB | — | yes | §3.7 |
| W21 | Same seed, same machine, 1 vs 8 threads | bit-identical P(t) | exact | yes (skipped in serial builds, §1.2) | determinism |
| W22 | C++ vs NumPy vs Numba (fixed-step RK4, same grid, Jing parameters, 1 ms) | max relative deviation of P(t) | < 1e-10 | yes | benchmark parity |
| W24 | Cross-platform: same config and seed on Linux x86-64 and macOS arm64. (a) raw `mt19937_64` output and the 53-bit uniforms; (b) Box–Muller normals and Ω̃_th samples; (c) P(t) for the W22 case and for one W19 realization | (a) bit-identical; (b) rel. 1e-12; (c) rel. 1e-9 | as listed | no (needs both machines); required before results from both platforms share a figure | this work |

Why W24 is not bit-identical in (b) and (c): even with `-ffp-contract=off`, `std::exp`, `std::sin`, `std::log` and friends come from different maths libraries (glibc on Linux, Apple's libm on macOS), and Eigen vectorizes with different instruction sets (SSE on x86-64-v2, NEON on arm64). Differences start at the last bit and must stay at that level.

### 6.4 Parameter sets

**Jing et al., Nat. Phys. 2020 (W2, W5, W12, W13, W22).** Methods and Fig. 2:
- Cs 6S₁/₂ F = 4 → 6P₃/₂ F = 5 → 47D₅/₂ → 48P₃/₂; probe 852 nm, coupling 510 nm, counter-propagating.
- γ₂/2π = 5.2 MHz; γ₃/2π = 3.9 kHz; γ₄/2π = 1.7 kHz (blackbody transitions included up to n = 70).
- Ω_p/2π = 5.7 MHz; Ω_c/2π = 0.97 MHz.
- Cell 5 cm, room temperature; N0 = 4.89e10 cm⁻³.
- Radial matrix element 2946.512 e·a0; dipole μ_RF = 1443.450 e·a0.
- LO at 6.94 GHz; δ_s = 150 kHz; E_LO optimum 3.0 mV/cm **rms** (Ω₁/2π = 7.9 MHz).
- Beam 1/e² diameters 1.70 mm (probe), 2.00 mm (coupling); probe incident 120 µW, about 10 µW at the detector.
- **Not given:** transit rate γ_t and probe dipole μ₁₂. Derive γ_t from beam size and σ_v with a stated formula and ask Aan to confirm; take μ₁₂ from ARC (record the call).

**Zhu & Dai 2025 (W8–W11, W19).** As `rydsim` TSD §6 (L = 2 cm, P0 = 29.8 µW, Ω_p = 2π·8.08 MHz, Ω_c = 2π·2.05 MHz, η = 0.8, f_LO = 6.9458 GHz, E_LO = 0.04 V/m, f_IF = 150 kHz). Decay rates are not printed; Zhu & Dai say they are aligned with Jing 2020, so use Jing's γ₂, γ₃, γ₄ and cross-check against ARC. Record this in `docs/deviations.md`. (This also answers `rydsim` FSD Q2 if Aan agrees.)

**Wu et al., PRA 2023 (W16, W17).**
- Cs 6S₁/₂ → 6P₃/₂ → nD₅/₂ → (n+1)P₃/₂ with n ≥ 15; extract n and the remaining parameters from Secs. II–III, or ask.
- T = 290 K; Ω_LO = Ω_opt; Δf₁ = 100 kHz, Δf₂ = 250 kHz.
- Spectrum case: Ω_p = 6 MHz, Ω_s = Ω_int = 500 kHz; blocking case: Ω_s = 1 kHz, Ω_int swept; d = 0.76 mm (meaning to be extracted).
- Their Rabi frequencies are quoted without an explicit 2π: resolve and record (§8 pitfall 5).

**Cui et al., TWC 2026 (W18, optional).** Cs, L = 5 cm, N0 = 5e8 cm⁻³; Ω_p = 2π·6 MHz, Ω_c = 2π·10 MHz (6S₁/₂ → 6P₃/₂ → 60D₅/₂); γ₂ = 2π·5 MHz; η = 0.8; reference Rabi frequencies 2π·4/√N MHz; a = 1e-3; δ_n = 2π·200n kHz; 1 ms window. Transitions and dipoles from their Table I.

**Chen/Yuen/See 2026 (W14, optional).** f_LO = 84.18 GHz, f_RF = 83.72 GHz. Other parameters are taken from their refs [27], [28], which are not in the project.

## 7. Convergence protocol for science figures (G6–G8)

For every plotted point, record in the figure's sidecar JSON the change in the plotted quantity when each of these is refined on its own:

| Knob | Refinement | Threshold |
| --- | --- | --- |
| Velocity grid | N_core ×2, N_w ×2, v_cut + 2σ_v | 0.1 dB (dB quantities) or 2% (ratios) |
| Integrator | rtol /10 (or dt /2 for RK4) | same |
| Warm-up | t_warm ×2 | same |
| Realizations | ×2 | same, or the 95% confidence interval reported |
| Thermal band | B_th ×2 | same |

A figure is final only when every point passes, or the failing points are marked on the figure. The sidecar also records the platform tag.

## 8. Known pitfalls

1. **Gauss–Hermite on narrow features.** EIT and the intermediate-state Lorentzian are about 0.01–0.05σ_v wide in velocity, while 64-node Gauss–Hermite nodes are about 0.4σ_v apart near the centre and reach ±14.9σ_v. Result: 87% error in W7a. The far nodes (44 of 64 lie beyond 4σ_v, with total weight 7e-5) also force very small explicit time steps (probe detuning up to about 2.4 GHz). **This also affects `rydsim` TSD §3.5, which specifies Gauss–Hermite with ≥ 64 nodes for V23–V25.** Flag it to Aan; do not change `rydsim` without his approval.
2. **Wu et al. Doppler weight:** they print limits "∫_{3u}^{3u}" (read as −3u…3u) and a weight exp(−v²/u²)/(√π u) with u = sqrt(kT/m). That weight corresponds to u = sqrt(2kT/m). We use σ_v = sqrt(kT/m) with the correct Gaussian and record the deviation (FSD Q6).
3. **Zhu & Dai:** σ_v² printed as sqrt(kT/m) (typo; `rydsim` §8) and double-sided PSDs (convert at the boundary).
4. **Jing 2020 uses rms fields.** The quoted Ω₁/2π = 7.9 MHz corresponds to the peak amplitude (W5).
5. **"Ω = 6 MHz" without 2π** (Wu; Hybrid six-level paper uses "units of 2π × MHz"). Resolve per source and record.
6. **Shared thermal realization:** all velocity classes see the same field realization. Independent fields per class would wrongly average the noise down.
7. **Spectral leakage:** use coherent sampling for tones (§3.9). A Hann window on a tone that has an integer number of cycles still biases its amplitude, so tones and noise are analysed separately.
8. **Explicit-integrator stability** is set by the largest detuning in the velocity grid, not by the physics of interest. Use per-class dt (§3.5) or DOPRI5.
9. **Classical field has no vacuum term** (§3.7); about 0.1% at 6.9458 GHz.
10. **RIN back-action** on the atoms through Ω_p is neglected; treat it as a stated limitation in the paper.
11. **No `-ffast-math` or `-Ofast`.** They reorder floating-point sums and break W21 and NaN checks.
12. **Warm-up:** Rydberg population decays are kHz-scale; starting from the static steady state avoids waiting for them, but the t_warm convergence check is still required.
13. **FMA contraction differs between machines.** Clang fuses `a*b + c` into one FMA by default where the CPU has it (arm64 always; x86-64 only from v3). The M2 and the Intel PC then round differently. `-ffp-contract=off` (§1.1) removes this source. It does not remove differences from the maths library and SIMD width (W24).
14. **`std::normal_distribution` is not portable.** The C++ standard fixes the `mt19937_64` sequence but not the algorithm that turns it into normal or uniform doubles. libstdc++ and libc++ give different thermal-field realizations from the same seed. Use the own converters in `rng.hpp` (§3.7).
15. **Rosetta on macOS.** An x86 CMake or Homebrew under Rosetta reports `x86_64`, adds `-march=x86-64-v2`, and builds an Intel binary that runs emulated. Check with `uname -m` and `cmake --system-information | grep CMAKE_SYSTEM_PROCESSOR`; both must say `arm64`.

## 9. Performance notes (measured, not assumed)

The engine's cost scales as (velocity classes) × (time steps per class) × (RHS cost) × (realizations) × (sweep points). From W7a, the velocity grid needs on the order of 10²–10³ classes, not 64, which makes the time-domain problem heavier than the earlier estimate. Wing classes need small steps; the quasistatic wing option (§3.4) and per-class adaptive steps are the main levers.

Measure before optimizing:
1. Profile one realization: `perf` on Linux (release build with frame pointers), Instruments → Time Profiler on macOS.
2. Report time per class, steps per class and rejected steps (DOPRI5).
3. Optimize only what the profile shows: ladder sparsity in the commutator, avoiding temporaries in Eigen expressions, memory layout of per-class states.

T2 reports measured times only, all from **one named machine** (CPU model, core count, OS, compiler and version, flags, OpenMP on/off). Timings from the second machine may appear as a separate, clearly labelled column. No speed-up claim in the paper without a measurement in T2.