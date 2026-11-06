# Phase 5B hardening audit

**Branch:** `fix/phase-5b-completion-and-hardening`, based on `origin/main`
`483e4c7f4c43d5e78839dc92a4e1be59ae45660a`.

The register distinguishes code and regression coverage from platform evidence.
Windows checks are locally VERIFIED. Linux and sanitizer code paths are also
VERIFIED by the corrective hosted run documented below. GUI edge cases and
dense repaint latency remain NOT VERIFIED.

| ID | Finding | Reproduction | Fix | Regression test | Verification |
|---|---|---|---|---|---|
| F-01 | POSIX PDF timeout | A fake executable that sleeps beyond `process_timeout` previously blocked the PDF adapter in blocking `waitpid()`. | `src/datasim/pdf/process_capture.cpp`: POSIX monotonic nonblocking wait, EINTR retry, bounded polling, child reaping, and safely owned process-group termination. Windows capture uses a Job Object and terminates residual descendants. | `tests/unit/pdf_process_tests.cpp` with `tests/unit/pdf_process_fixture.cpp`: success, nonzero exit, missing/non-executable file, timeout, descendant cleanup on POSIX, exact/output quotas, and concurrent calls. | VERIFIED on Windows and Ubuntu 24.04 in hosted run [42170539048](https://github.com/quocthaile/datasim/actions/runs/42170539048), including Linux timeout and descendant cleanup. No cancellation behavior is claimed. |
| F-02 | Strict PDF geometry | Malformed coordinate tokens such as `10..2`, `1.2.3`, `1e999`, `NaN`, or `Infinity` were accepted by numeric-prefix parsing or not reliably rejected. | `src/datasim/pdf/table_extraction.cpp`: consume the complete quoted coordinate, require finite values, and require positive-area boxes. Source markup and extraction provenance paths remain as before. | `tests/unit/pdf_coordinate_tests.cpp` calls the production parser through `src/datasim/pdf/table_extraction_test.hpp`; covers required malformed forms and valid negative, decimal, exponent coordinates, plus inverted/zero-area boxes. Existing `pdf_extraction` covers real Poppler fixtures. | VERIFIED on Windows and Ubuntu 24.04: coordinate tests pass; Linux `pdf_extraction` also passed against real Poppler in run [42170539048](https://github.com/quocthaile/datasim/actions/runs/42170539048). |
| F-03 | Incomplete source fingerprint | Application/test headers and synthetic inputs were omitted from identity coverage. | `CMakeLists.txt`: sorted recursive source scope includes `src`, `apps`, `tests`, `cmake`, CI scripts/workflows, and top-level CMake config; committed synthetic PDF allowlist is hashed as binary; text CRLF is normalized; build outputs, transient caches and other binary/vendor files are excluded. `DATASIM_GIT_REVISION` remains separate. Fingerprints are not signatures or authentication. | `scripts/ci/verify_source_fingerprint.py` mutates/restores `apps/datasim_viewer/waveform_widget.hpp` and `tests/fixtures/synthetic_datasheet.pdf`, checks an excluded build-directory probe, and repeats unchanged configure. | VERIFIED on Windows: unchanged and exact restored tree produced `e8d69bb77c84d6dac3f844f379ce0142030868fd63c447d1f804ee0480adc07d`; each selected source mutation changed identity and build-directory probe did not. Cross-platform path-equivalence remains pending Linux. |
| F-04 | PDF runtime output limit | A backend can emit beyond the previously post-read-only limit before extraction rejects its output. | `src/datasim/pdf/process_capture.hpp/.cpp` bounds persisted/captured output during execution, enforces process timeout, and reports output-limit status. `src/datasim/pdf/table_extraction.cpp` routes `pdftotext` stdout into the bounded capture and applies an aggregate 33 MiB process-output cap; selected text retains the 32 MiB semantic cap. `pdfinfo` is capped at 1 MiB. | Process fixture writes beyond configured stdout/stderr quota; tests assert limit status and persisted size. Existing PDF extraction exercises bounded output handling. | VERIFIED on Windows and Ubuntu 24.04: exact limit, excess stdout/stderr, and Poppler extraction pass in run [42170539048](https://github.com/quocthaile/datasim/actions/runs/42170539048). |
| F-05 | CSV memory and rendering | Large CSVs previously loaded synchronously and dense vectors could create unbounded drawing-path work relative to viewport width. | `apps/datasim_viewer/read_only_samples.cpp` reads at most 16 MiB and rejects more than 1,000,000 samples before the core parser; `csv_load_worker.*` and `main.cpp` load asynchronously. `waveform_widget.*` selects source-ordered per-pixel extrema for drawing and preserves the full `datasim::Samples`. | `tests/desktop/viewer_sample_tests.cpp` covers valid, missing, malformed, non-finite, repeated and one-million-row loads. Qt worker test covers queued delivery. | VERIFIED on Windows desktop: 14/14 tests, 0 skips; 1,000,000 samples loaded in MEASURED 7,572 ms. The benchmark measures loading, not paint latency; dense repaint timing and memory profiling are NOT VERIFIED. |
| F-06 | GUI lifecycle | The original workbench's compile and service tests did not prove the visible user flow or asynchronous CSV path. | `apps/datasim_viewer/main.cpp` integrates `CsvLoadWorker`, disables conflicting controls during loads/runs, receives results on the GUI thread, and defers close while workers are active. Simulation worker lifecycle remains the existing Phase 5B pattern. | `tests/desktop/viewer_worker_tests.cpp` (existing) and viewer CSV tests exercise Qt thread/queued result delivery; manual workbench was launched and interacted with. | VERIFIED, bounded on Windows: default real ngspice run, metrics/artifacts, Vin toggle, and read-only generated CSV display observed. Invalid-input visual behavior, backend failure screen, close-during-run, and dense-render responsiveness remain NOT VERIFIED. |
| F-07 | Linux/sanitizer CI gap | Existing workflow had Windows validation only. | `.github/workflows/rc-validation.yml` adds Ubuntu 24.04 core integration and ASan/UBSan jobs, records dependency versions, runs synthetic PDF generation and CTest, and rejects JUnit-reported skips. | Hosted workflow is the regression configuration; local Linux is unavailable in this Windows environment. | VERIFIED in hosted run [42170539048](https://github.com/quocthaile/datasim/actions/runs/42170539048): Linux core and sanitizer jobs succeeded, each with 11/11 tests; JUnit skip guard passed. |

## Reproduction and current evidence

Windows Release build and CTest commands were executed in the hardening
worktree. Core: 11/11 passed, 0 failed, 0 skipped. Qt-enabled: 14/14 passed,
0 failed, 0 skipped. The Qt-enabled CTest includes actual Poppler extraction,
synthetic corpus evaluation, real ngspice integration, process capture limits,
coordinate parsing, CSV loading, and worker delivery. The exact environment,
measured CSV timing, GUI observations, and numerical values are recorded in
[PHASE_5B_VALIDATION.md](PHASE_5B_VALIDATION.md).

Linux POSIX process behavior and ASan/UBSan are VERIFIED by hosted run
[42170539048](https://github.com/quocthaile/datasim/actions/runs/42170539048).
GUI edge cases and dense repaint latency remain NOT VERIFIED. No physical
measurement, vendor datasheet, or new licensing claim is made.

## PR #17 Linux CI failure follow-up

The first integrated hosted run, [42165152332](https://github.com/quocthaile/datasim/actions/runs/42165152332),
compiled all three jobs. `windows-ucrt` succeeded; `linux-core` and
`linux-sanitizers` each failed two CTest cases. The failures were test/process
contract defects rather than PDF interpretation or sanitizer findings:

1. `tests/unit/pdf_extraction_tests.cpp::locate()` split `PATH` only on `;`
   and searched only for `.exe` tools, so Ubuntu's colon-separated PATH and
   `pdfinfo`/`pdftotext` names were never found.
2. `tests/unit/pdf_process_tests.cpp` required an exception for a nonexistent
   executable. POSIX `execv()` instead failed in the child and returned status
   127, while Windows `CreateProcessW()` threw. The contract was inconsistent.

The correction makes `locate()` use `;` plus `.exe` on Windows and `:` with
unsuffixed executable names on POSIX. `run_capture()` now validates that the
executable is a regular file before process setup and, on POSIX, checks
`X_OK`; both platforms throw for a missing path, and POSIX throws for a
non-executable file. A successfully launched program's exit status remains
unchanged, including nonzero exits. Timeout/process-group handling and runtime
quotas are retained. Tests also cover excessive stdout as well as stderr.

Corrective hosted results are VERIFIED in [run 42170539048](https://github.com/quocthaile/datasim/actions/runs/42170539048):
Windows UCRT, Linux core, and Linux ASan/UBSan all succeeded. Exact test counts
and zero-skip evidence are in [PHASE_5B_VALIDATION.md](PHASE_5B_VALIDATION.md).
