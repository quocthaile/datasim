# Phase 5B implementation record

**Status:** Original workbench IMPLEMENTED on `feat/phase-5b-rc-simulation-workbench` and ported onto `483e4c7`. Incremental hardening is IMPLEMENTED on `fix/phase-5b-completion-and-hardening`. Windows core/desktop checks and the primary manual GUI path are VERIFIED locally; hosted Linux and sanitizer checks are NOT VERIFIED pending branch CI.

## Implemented

- Extended existing `datasim_viewer` into a `Manual Ideal-RC Simulation`
  workbench, retaining `WaveformWidget` and the read-only Open CSV workflow.
- Added strict locale-independent decimal/scientific parsing for all eleven
  `RcConfig` fields and authoritative `datasim::check_config()` validation.
- Added a `SimulationWorker` moved to a `QThread`; the worker owns values and is
  the only production UI path that calls `datasim::execute_rc_run()`.
- Added close deferral while a worker is active; no detached thread or
  cancellation is used. Run/Open controls are disabled during execution.
- Added non-creating fresh run path selection and reports artifact existence
  after success or failure.
- Displays returned samples and numerical metrics only after the existing
  execution/validation path succeeds. Failure clears the plot; CSV-loaded data
  retains a separate label.
- Added application tests for defaults/custom values, malformed and non-finite
  numbers, core configuration constraints, fresh paths, real backend runs,
  artifact existence, validation report fields, and collision preservation.
- Added a Qt event-loop worker test that verifies real execution occurs through
  `SimulationWorker` and validated results arrive on the main event thread.
- Added a Qt-enabled hosted CI build/test step while retaining the default Qt-off
  core configure and test path.
- Added asynchronous read-only CSV loading with a 16 MiB input cap and a one
  million sample cap; dense waveform painting builds an extrema-preserving
  display path bounded by plot width while retaining original `Samples`.
- Hardened `datasim_pdf` process capture with execution time and output limits,
  process-tree cleanup, and strict finite full-token coordinate parsing with
  positive-area box validation.
- Expanded `DATASIM_SOURCE_SHA256` to deterministically cover production,
  application, tests, CMake/CI scripts, and selected committed synthetic PDF
  inputs. Text line endings are normalized; binary fixture bytes are hashed.
- Added Linux core and ASan/UBSan CI jobs; hosted outcomes are pending.

## Not implemented

No generic JSON parsing, metadata editing, `datasim_vertical` workflow, component
approval, arbitrary netlist editing, cancellation, physical measurement, or
automated interactive GUI test is included. The manual GUI check covers launch,
default real ngspice execution, result display, Vin overlay, and read-only CSV
loading; close-during-run and visible invalid/failure-state interactions remain
NOT VERIFIED.
