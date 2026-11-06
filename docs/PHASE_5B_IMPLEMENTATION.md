# Phase 5B implementation record

**Status:** IMPLEMENTED on `feat/phase-5b-rc-simulation-workbench`; hosted CI passed as recorded in [validation](PHASE_5B_VALIDATION.md). Manual GUI inspection remains NOT VERIFIED.

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

## Not implemented

No generic JSON parsing, metadata editing, `datasim_vertical` workflow, component
approval, arbitrary netlist editing, cancellation, physical measurement, or
automated interactive GUI test is included.
