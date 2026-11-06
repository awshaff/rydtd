# FSD — `rydtd`: C++ time-domain engine for the companion paper

**Paper (working title):** "Beyond the Small-Signal Model: Noise–Nonlinearity Interaction in Rydberg Atomic Superheterodyne Receivers"
**Target venue:** IEEE Trans. Commun. or IEEE TMTT (open, see §9 Q1). Engine as a separate software paper (JOSS) is optional.
**Owner:** Aan (Abdurrahman Wachid Shaffar) · **Version:** 0.1 · **Date:** 2026-11-05
**Companion documents:** `TSD.md` (equations, conventions, validation targets), `AGENTS.md` (instructions for the coding agent, including the learning protocol)
**Sibling project:** `claude/rydsim/` (JSAC paper, Python). `rydtd` follows the `rydsim` conventions and supplies its stochastic check (rydsim V32).

---

## 1. Purpose

`rydtd` is a C++20 engine, with Python bindings, that integrates the Doppler-averaged Lindblad master equation of a Rydberg atomic superheterodyne receiver **directly in the time domain**, with arbitrary drives and a stochastic thermal field. It produces every number, table and figure in the companion paper from one reproducible codebase.

It has two goals with equal weight:

1. **Research:** answer questions that the published linear and weakly nonlinear models cannot.
2. **Learning:** be the vehicle through which Aan learns research-grade C++ (see `AGENTS.md` §3). Code quality, tests and explanations matter as much as speed.

### 1.1 The gap

| Model | Dynamic | Nonlinear | Thermal field | Doppler |
| --- | --- | --- | --- | --- |
| Wu et al., PRA 2023 (linear dynamic range) | No (steady state, envelope) | Full | No | Yes |
| Zhu & Dai 2025 (arXiv 2506.23455) | Yes | No (first order) | Linear PSD | Yes |
| Chen/Yuen/See 2026 (arXiv 2602.13955) | Yes | Third-order Volterra | Linear NEF | Not stated (check) |
| Cui et al., TWC 2026 (multi-band) | Yes | Linearized, checked against an ODE solver | No | Not in the ODE check (check) |
| **This work** | **Yes** | **All orders** | **Stochastic, time domain** | **Yes** |

No published model treats noise and nonlinearity together. In every model above, noise is propagated linearly, and nonlinearity is studied without noise.

### 1.2 Research questions

- **RQ-A, desensitization.** A strong in-band interferer compresses the atomic gain. How much does the input-referred noise (thermal plus internal) rise, as a function of interferer field and frequency offset? Define a blocking point and compare it with Wu et al.'s noiseless anti-blocking metric Ω_int3.
- **RQ-B, noise conversion by strong tones.** With a strong tone present (own signal or interferer), does the atomic nonlinearity mix thermal noise from other offsets into the IF band? Quantify the ratio of measured IF noise to the linear prediction |κ(iω)|²·S_E,th (signal plus image sidebands).
  - Expected null result without strong tones: the in-band thermal field over 1 MHz at 6.9458 GHz is about 1.2e-4 V/m peak, against an optimal LO of about 0.42 V/m peak (Jing 2020). Second-order products are therefore about 71 dB below the linear term. The engine must confirm this null. It supports the linear treatment in the JSAC paper.
- **RQ-C, validity of the Volterra model.** Compare the third-order Volterra kernels (Chen/Yuen/See method, applied to the four-level superhet) with the full solution. Map where IMD3/IIP3 predictions break down. This is a validation result, not the main claim.
- **RQ-D, modulated signals.** EVM of a QAM signal versus signal power, with thermal noise and with an interferer. Connects the results to the wireless claims of Gong, Cui and others.

## 2. Relation to `rydsim` and the JSAC paper

- **Boundary:** the JSAC paper covers fair noise accounting in the linear regime. This paper covers everything beyond the linear regime. The JSAC paper may cite this one for the time-domain check of its first-order thermal model.
- **Shared conventions:** `rydtd` uses `rydsim` TSD §2 unchanged (SI units, peak amplitudes referred to the atoms, single-sided PSDs, T0 = 290 K).
- **Shared data:** atomic data (dipoles, decay rates) are computed in Python (ARC) and passed to the engine as numbers. The C++ code never calls ARC.
- **Shared physics module:** the electronic readout chain (photon shot noise, RIN, TIA, Johnson) stays in `rydsim/atomic/noise.py`. `rydtd` returns the probe power P(t) with the atomic and thermal-field dynamics only; electronic noise is added in Python (TSD §3.8).
- **Deliverable to `rydsim`:** a Python call that `rydsim`'s `thermal/first_order.py` uses to pass rydsim V32 (= `rydtd` W19).

## 3. Users

| User | Needs |
| --- | --- |
| Aan (author, C++ learner) | Build the engine in stages, understand every part, generate the paper's figures |
| Advisor / co-authors | Rerun everything with one command; read the validation report |
| Reviewers (public repo at submission) | Reproduce every number; see where every parameter comes from |
| `rydsim` | Import the engine for V32 |
| Experimental groups (later) | Run the two-tone and blocking protocols (FR-8) with their own parameters |

## 4. Scope

**In scope:**
- Lindblad model for N = 3–6 levels with N fixed at compile time and a coupling graph given in the config (ladder for the superhet; star-shaped for the multi-band case of Cui et al.).
- Steady state, linear response H₁(iω), and third-order Volterra kernels (for validation and RQ-C).
- Time-domain integration with fixed-step RK4 and adaptive Dormand–Prince 5(4).
- Doppler averaging over a convergence-tested velocity grid (TSD §3.4).
- Drives: LO, tones, multi-tone, interferers, sampled complex envelopes (modulated signals).
- A complex Gaussian thermal field, band-limited, with the PSD of `rydsim` TSD §3.1.
- Probe readout by Beer–Lambert (optically thin slab by default).
- Analysis: PSD, IF demodulation, P1dB, IMD3/IIP3, desensitization, noise-conversion ratio, EVM.
- Python bindings (pybind11), a benchmark against NumPy and Numba, a validation report.

**Out of scope:**
- Experimental data acquisition or fitting raw data (no lab access).
- Many-body effects (Rydberg blockade, dipole–dipole interaction), cold-atom dynamics.
- Probe propagation beyond a slab model (optional later: x-slices with probe depletion).
- Gaussian radial beam averaging (optional, only if W23 is attempted).
- Probe intensity noise acting back on the atoms (RIN is treated as detector-side only; see TSD §8).
- GPU execution (possible later; not needed for the paper).

## 5. Functional requirements

### FR-1 Model core
- Build H(t)/ħ in the rotating frame from a coupling graph: levels, field per edge, detunings, Doppler wavevector per optical edge.
- Build the Lindblad dissipator: population decays, coherence dephasing, transit rate γ_t, repopulation of the ground state γ_in.
- All parameters from config; the config records a source for each.

### FR-2 Steady state and frequency-domain response
- Steady state per velocity class and Doppler-averaged.
- Linear response H₁(iω) from the Liouvillian (cross-check of the time-domain engine and of Zhu & Dai).
- Third-order kernel H₃ and the derived IIP3 estimate (Chen/Yuen/See Eqs. 54–57), for RQ-C.

### FR-3 Time-domain integration
- RK4 (fixed step) and Dormand–Prince 5(4) (adaptive), with tolerances as named config parameters.
- Each velocity class is an independent ODE, integrated in parallel (OpenMP).
- Start each class from its steady state under the static drives (LO only), then a named warm-up time.
- Output: Doppler-averaged ρ₂₁(t) and P(t) on a uniform output grid.

### FR-4 Drives
- LO plus any number of tones (amplitude, offset, phase) on any RF edge.
- Sampled complex envelopes for modulated signals (QAM with raised-cosine pulses; symbol stream from a seeded generator).
- Interferers as tones or modulated envelopes.

### FR-5 Stochastic thermal field
- Generate a complex Gaussian envelope with two-sided PSD equal to `rydsim`'s single-sided peak-convention PSD at f_LO + Δf (TSD §3.7), band-limited to a configurable bandwidth B_th around the LO (signal and image sides included).
- Reproducible: one seed per realization, independent of the thread count.

### FR-6 Readout
- Probe power P(t) via Beer–Lambert with the absorption coefficient of `rydsim` TSD §3.5.
- Return P(t) without electronic noise; the Python layer adds detector noise with `rydsim`'s noise module.

### FR-7 Analysis (Python side, reading engine output)
- Welch PSD (single-sided, stated window and ENBW); complex IF demodulation.
- P1dB with Wu et al.'s definition; IMD3/IIP3 from two-tone runs.
- Desensitization ΔNEF and a blocking point (TSD §3.9).
- Noise-conversion ratio R_conv (RQ-B).
- EVM after one-tap least-squares equalization.

### FR-8 Testable-prediction protocols
- Scripts that output, for user-supplied setup parameters, the predicted two-tone IMD3 curve, the blocking curve and the noise-conversion ratio, in the units a lab measures (field at the cell, IF power in a stated RBW). These go in the paper as falsifiable predictions.

### FR-9 Python bindings and `rydsim` integration
- `import rydtd` exposes the engine; configs are Python dicts built from YAML.
- `rydsim` V32 runs through `rydtd`.

### FR-10 Benchmark
- Same problem in C++, vectorized NumPy and Numba: identical results within tolerance (W22), timing versus thread count. Reported as a table in the paper or its appendix.

### FR-11 Reproducibility
- One command regenerates all figures, tables and `out/validation_report.md`.
- Cached results keyed by config hash and engine version.

## 6. Deliverables

| ID | Content | FR | Validation |
| --- | --- | --- | --- |
| G1 | Steady-state EIT–AT spectra and |κ| vs E_LO (Jing 2020 parameters) | 1, 2 | W5, W12 |
| G2 | Small-signal time-domain response vs H₁(iω); step response vs Zhu & Dai | 2, 3 | W8–W11 |
| G3 | Volterra-3 vs full solution: fundamental and IMD3 vs Ω_s; breakdown map over (Ω_s, Ω_LO) | 2, 3, 4 | W15 |
| G4 | Compression and anti-blocking trends vs Ω_p, cold and thermal (Wu 2023) | 3, 4 | W16, W17 |
| G5 | Stochastic IF noise PSD vs |κ|²·S_E,th (rydsim V32) | 5 | W19, W20 |
| G6 | RQ-A: ΔNEF vs interferer field and offset; blocking contour | 3, 4, 5, 7 | convergence checks |
| G7 | RQ-B: R_conv vs signal/interferer level and B_th; null case marked | 5, 7 | convergence checks |
| G8 | RQ-D: EVM vs signal power, with and without interferer | 4, 5, 7 | convergence checks |
| G9 | Benchmark: runtime vs implementation and threads | 10 | W22 |
| T1 | Validation table | all | — |
| T2 | Benchmark table | 10 | W22 |
| T3 | Parameter table with sources | all | — |

Figures use `rydsim`'s IEEE style (`rydsim/plotting/style.py`).

## 7. Acceptance criteria

1. One command (`make all`) builds the engine, runs tests and produces G1–G9, T1–T3 and `out/validation_report.md` from a clean checkout.
2. Every validation target in TSD §6 marked *blocking* passes within its tolerance. A failing target blocks the release; targets are never edited to pass.
3. Every physical parameter carries a source (paper + Eq./Table/page, fundamental constant, ARC call, or "user input").
4. Every science figure (G6–G8) comes with a convergence record: velocity grid, time step or tolerance, warm-up time and number of realizations, each varied until the plotted quantity changes by less than the stated threshold (TSD §7).
5. Results are bit-identical for the same seed on 1 and 8 threads (W21).
6. Builds warning-free with `-Wall -Wextra -Wpedantic` on GCC and Clang; tests pass under AddressSanitizer and UndefinedBehaviorSanitizer.
7. Full figure set runs in under 12 h on 8 cores with a cold cache (tracked, not blocking for v0.1).

## 8. Assumptions

- C++20; GCC ≥ 11 or Clang ≥ 14; CMake ≥ 3.24; Eigen 3.4; OpenMP; Catch2 v3; pybind11 ≥ 2.11; pocketfft (header-only, BSD).
- Python ≥ 3.11 with `rydsim`'s pinned stack, plus Numba (benchmark only) and scikit-build-core (packaging).
- Licence: permissive (BSD-3 or MIT). GPL dependencies (e.g. FFTW) are avoided so the licence stays permissive.
- Source papers are read from the QRAR project knowledge (mirrored into `refs/`).

## 9. Open questions

| # | Question | Owner |
| --- | --- | --- |
| Q1 | Venue: TCOM (RQ-A and RQ-D lead) or TMTT (receiver metrics lead)? Affects figure order and emphasis, not the code | Aan / advisor |
| Q2 | Chen/Yuen/See take parameters from their refs [27], [28], which are not in the project. Add those papers, or validate RQ-C only against our own Volterra code (W15)? | Aan |
| Q3 | Optically-biased receiver (2025) superhet saturation at 1.84 mV/cm needs the Rb Rydberg states and Gaussian-beam averaging. Attempt W23, or leave it out? | Aan |
| Q4 | Cui et al. multi-band check (W18) needs their Table I and values read off Fig. 8. Worth digitizing? | Aan |
| Q5 | QAM order, symbol rate and roll-off for RQ-D: copy from Cui/Gong or choose a neutral set? | Aan |
| Q6 | Wu et al. print a Doppler weight that does not match their stated u = sqrt(kT/m) (TSD §8). Confirm we use σ_v = sqrt(kT/m) and record the deviation | Agent → Aan |