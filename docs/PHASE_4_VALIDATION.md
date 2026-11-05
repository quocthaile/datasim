# Phase 4B validation

Status: **IMPLEMENTED and VERIFIED** on the Phase 4B merge commit by hosted Windows CI
[run 41995104973](https://github.com/quocthaile/datasim/actions/runs/41995104973).
The local results below remain **NOT VERIFIED** for real ngspice because CTest
skipped it. These local results are for Windows
x64 with MSYS2 UCRT GCC
16.1.0, CMake 4.3.2, and the `MinGW Makefiles` generator.

## Commands and outcomes

```text
cmake -S . -B build/phase4b -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build/phase4b -j 4
ctest --test-dir build/phase4b --output-on-failure
```

Configure and build completed successfully. CTest passed six tests:

| Test | Result |
| --- | --- |
| `unit` | passed |
| `component_model` | passed |
| `vertical_slice` | passed |
| `intake` | passed |
| `intake_handoff` | passed |
| `backend_errors` | passed |
| `ngspice_rc` (including P-07) | **skipped; NOT VERIFIED locally** |

The aggregate CTest result was `100% tests passed, 0 tests failed out of 7`,
with one explicitly skipped integration test. No `DATASIM_NGSPICE` executable
was configured, so real-backend integration, the six-run study, and numerical
SHA-256 non-regression were not rerun here. Historical Phase 3 evidence remains
the reference for those checks.

Hosted run 41995104973 completed its real backend integration and convergence steps
successfully. The workflow fails if the required integration test is skipped.

## Hardening coverage

The Release test executables use explicit throwing `require` checks rather than
`assert`, so checks remain active when `NDEBUG` is defined. The intake suite
reports 47 executed checks and the handoff suite reports 14 executed checks.
The intake suite covers P-01/P-02/P-03/P-04/P-05/P-06 and
the observable portions of N-01/N-02/N-03/N-04/N-05/N-06/N-07/N-08/N-12/
N-13/N-14/N-16/N-17/N-18/N-20/N-21/N-24/N-33/N-43/N-44. Cases requiring
unrepresented source-cell provenance, embedded document content, or a
manufacturer-specific suffix grammar remain BLOCKED/NOT IMPLEMENTED.

The handoff suite covers candidate rejection before normalization/promotion,
invalid direct approval, valid two-step human review, invalid reviewer kind,
missing rationale, aggregate evidence-reference failure, normalized-parameter
preservation, knowledge promotion, domain validation, and synthetic provenance.

## Reconciled test semantics

- Hard validation errors prevent candidate bundle construction.
- Ambiguous raw values without an asserted number are recoverable warnings and
  remain visible in the candidate.
- Review-required metadata is not treated as approval.
- Page-index derivation is testable only when the intake entry records that it
  occurred.
- Source-cell, qualifier, category, footnote, and embedded-content policies are
  checked only where the intake contract represents the relevant metadata.
