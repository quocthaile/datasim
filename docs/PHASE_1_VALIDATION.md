# Phase 1 validation

Status: **VERIFIED**, scoped to the implemented ideal RC transient and the tested
Windows/GCC/ngspice environment. Numerical results were collected on 2026-10-31.
No physical measurements were acquired. The charter reserves **MEASURED** for
physical observations, so these are recorded as verified computed results.

## Circuit and reference

V1 drives R1 from `vin` to `vout`; C1 connects `vout` to ground. All values are
manually configured ideal components, not datasheet-extracted knowledge.

| Parameter | Value |
| --- | --- |
| R1 | 1000 ohm |
| C1 | 1e-6 F |
| Initial capacitor / source voltage | 0 V / 0 V |
| Source high level | 5 V |
| Delay | 0.001 s |
| Rise / fall | 1e-9 s / 1e-9 s |
| Width / period | 0.010 s / 0.020 s |
| Transient requested step / maximum integration step | 1e-5 s / 1e-5 s |
| End time | 0.006 s |
| Temperature / nominal temperature | 27 degC / 27 degC |
| Solver controls | trapezoidal, reltol=1e-6, abstol=1e-12 A, vntol=1e-9 V |

The generated PULSE source is explicit. `.ic V(vout)=0` and capacitor `IC=0`
specify the initial voltage. The normal zero-source operating point is used
without UIC. ngspice's actual initial solution has vin=vout=0. The 6 ms simulation
covers five RC time constants after the 1 ms delay and ends before discharge.

The independent analytical reference is:

```text
tau = R*C = 0.001 s
Vref(t) = 0                                      for t <= td
Vref(t) = Vs * (1 - exp(-(t-td)/tau))             for t > td
```

Implementation uses `-Vs*expm1(-(t-td)/tau)` for numerical stability near the step.
It takes only configuration and time; no simulated voltage contributes to the
reference. At one tau after the delay the expected voltage is 3.160602794142788 V;
at 6 ms it is 4.966310265004573 V. Unit tests check these independent constants.

## Method and tolerance fixed before execution

Compare at actual adaptive ngspice sample times. The inclusive validation interval
is `[td + rise, duration] = [0.001000001, 0.006] s`. The finite transition interval
is excluded. Pre-step output is separately checked against zero and all input
samples against the configured finite-ramp pulse. The ideal step is not the exact
finite-ramp solution: for this 1 ns rise and 1 ms tau its maximum post-rise
discrepancy is bounded by approximately `Vs*rise/(2*tau) = 2.5 microvolt`.
Excluding the ramp does not eliminate this small modeling difference.

For each of N post-transition samples:

```text
e_i = Vout_sim(t_i) - Vref(t_i)
absolute_error_i = abs(e_i)
maximum_absolute_error = max(abs(e_i))
RMSE = sqrt(sum(e_i^2)/N)
normalized_RMSE = RMSE / abs(Vs)
```

RMSE is sample-weighted on the adaptive grid, not time-weighted. Full time coverage,
maximum sample gap, finite values, strict monotonicity and nonempty output are
prerequisites; truncation cannot silently improve the metric.

Acceptance is **maximum absolute error <= 0.01*5 V = 0.05 V**. This task-specified
1% amplitude threshold is a coarse feasibility criterion, fixed before running;
it allows the small finite-rise discrepancy and numerical integration error.
RMSE is reported for characterization, not used as a second acceptance threshold.
The tolerance is not a device-accuracy guarantee and is not tuned to the result.

## Actual run and evidence

Backend: **ngspice-47**, MSYS2 package 47-1, build banner dated
`Mon Sep 7 15:42:04 UTC 2026`. It is compiled with KLU support; the actual RC log
reports **SPARSE 1.3** as the active direct linear solver. Integration is a separate
console executable process. Compiler: GCC 16.2.0 (MSYS2 UCRT Rev3), C++20, Release;
CMake 4.4.0-rc2 with Ninja on Windows x64. Exact commands and runtime provisioning
are in [implementation](PHASE_1_IMPLEMENTATION.md).

| Collected result | Value |
| --- | --- |
| Backend exit status | 0 |
| Retrieved / exported samples | 640 |
| Validation samples | 525 |
| Maximum absolute error | 1.429824283105674e-5 V |
| RMSE | 8.487004837183902e-6 V |
| Normalized RMSE | 1.6974009674367804e-6 |
| Tolerance | 0.05 V |
| Numerical validation | PASSED |
| Final sample time / input / output | 0.006 s / 5 V / 4.966311647419354 V |
| Final analytical voltage | 4.966310265004573 V |

Reviewable captured artifacts:

- [Generated netlist](../examples/rc_transient/reference-run/circuit.cir)
- [Normalized results](../examples/rc_transient/reference-run/results.csv)
- [Pointwise analytical errors](../examples/rc_transient/reference-run/errors.csv)
- [Run metadata](../examples/rc_transient/reference-run/metadata.txt)
- [Backend output](../examples/rc_transient/reference-run/backend-output.txt)

Raw `samples.dat` and the full version query are retained locally in
`build/rc-final/`; they are not required as input to rerun the spike. Captured CSVs
are evidence only: integration tests execute ngspice afresh and never read them
as expected backend output. A separate Python standard-library CSV/math audit
confirmed 640 finite, strictly increasing rows and independently reproduced both
error metrics from the normalized CSV.

## Automated and negative checks actually run

Final `ctest --test-dir build/phase1 --output-on-failure -V`: **3/3 tests passed,
zero skipped** after provisioning ngspice.

| Test | Evidence |
| --- | --- |
| `unit` | 59 checks passed: tau, known analytical values, invalid R/C/voltage, unsupported timing/IC, netlist generation/parameter changes, known error metrics, failing tolerance, incomplete coverage, wrong input, CSV roundtrip and malformed/empty/nonfinite parsing. |
| `backend_errors` | Passed: missing executable, failed launch, nonzero backend exit, missing output, warning, empty output, malformed and nonfinite data. The helper process is a fault fixture, not a numerical simulator. |
| `ngspice_rc` | Passed: real ngspice execution, 640 samples, CSV write/read, post-transition tolerance and rejection of stale artifacts. Output directories include spaces. |

Before runtime provisioning the integration test explicitly skipped (exit 77);
that earlier skip was not counted as successful integration. The relocated MSYS2
runtime initially emitted code-model loading errors; the adapter rejected the run.
After correcting only the extracted runtime's paths, a fresh run passed without
those diagnostics. No errors were ignored to obtain the successful result.

Additional CLI checks: R=-1 failed before creating a run; an explicit missing
executable failed with both statuses `NOT_RUN`. A separate configured run with
R=2000 ohm and Vs=3 V (other defaults unchanged) passed with maximum error
2.03135085064e-6 V, RMSE 1.5853601048e-6 V, tolerance 0.03 V. Its artifacts remain
in `build/rc-custom/`.

## Limitations and remaining questions

- Only this Windows/GCC/ngspice build is VERIFIED. POSIX code and MSVC builds are
  IMPLEMENTED/intended respectively but have not been tested. No cross-backend comparison ran.
- No physical measurements, datasheet extraction, fitted device model or hardware
  accuracy claim is involved. Ideal RC success does not validate advanced models.
- The ideal-step reference is valid only for the supported initial charging
  interval. Nonzero IC, negative/zero step amplitude and later pulse edges are unsupported.
- System `spinit` and runtime DLLs remain external; `-n` suppresses user spiceinit,
  not all installed initialization. Exact netlist, version, settings and diagnostic
  logs are retained; bitwise cross-platform reproducibility is not promised.
- Diagnostic keyword rejection is intentionally conservative and can reject a
  benign warning. Timeout is implemented but a forced-hang test was not run.
- Process execution and metadata are a small CLI implementation, not a sandbox,
  production job scheduler or arbitrary untrusted-netlist service.
- Repository licensing and production distribution/licensing of the complete
  runtime remain owner decisions. No binary packages are vendored.

Phase 1 acceptance is met for the tested environment. No merge or Phase 2 work
has been performed.
