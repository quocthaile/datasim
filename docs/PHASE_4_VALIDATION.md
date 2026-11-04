# Phase 4B validation

Status: **IMPLEMENTED; locally VERIFIED for the checks listed below**. These
results are for the current working tree on Windows x64 with MSYS2 UCRT GCC
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
| `ngspice_rc` | **skipped; NOT VERIFIED locally** |

The aggregate CTest result was `100% tests passed, 0 tests failed out of 7`,
with one explicitly skipped integration test. No `DATASIM_NGSPICE` executable
was configured, so real-backend integration, the six-run study, and numerical
SHA-256 non-regression were not rerun here. Historical Phase 3 evidence remains
the reference for those checks.

No hosted CI result is claimed by this document.

## Reconciled test semantics

- Hard validation errors prevent candidate bundle construction.
- Ambiguous raw values without an asserted number are recoverable warnings and
  remain visible in the candidate.
- Review-required metadata is not treated as approval.
- Page-index derivation is testable only when the intake entry records that it
  occurred.
- Source-cell, qualifier, category, footnote, and embedded-content policies are
  checked only where the intake contract represents the relevant metadata.
