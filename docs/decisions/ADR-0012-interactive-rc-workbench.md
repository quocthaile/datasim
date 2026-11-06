# ADR-0012: Interactive ideal-RC workbench

**Status:** Accepted for the incremental Phase 5B application extension; project licensing and binary redistribution remain unresolved.
**Date:** 2026-11-06

## Existing architecture

Phase 5A added optional `datasim_viewer`, gated by `DATASIM_BUILD_DESKTOP=OFF`,
and the Qt 6 Widgets application dependency. The five production libraries and
their public interfaces are unchanged. The viewer loads local CSV with
`datasim::read_csv()` into `datasim::Samples` and draws it with the existing
`WaveformWidget`.

`datasim::RcConfig` is the manual ideal-RC input contract. `check_config()` is
the authoritative constraint check, and `time_constant()` returns R*C only
after validation. `execute_rc_run(config, executable, new_directory)` owns
exclusive creation of a previously absent destination, runs ngspice, writes
existing run artifacts, performs the existing numerical validation, and returns
`RunResult { Samples, Validation, backend_seconds }`. Its failure path preserves
the created run workspace and diagnostic metadata. `unique_workspace()` creates
a directory immediately, so its result cannot be passed as `new_directory`.

## Decision

Extend the existing `datasim_viewer` target. Keep Qt at the application layer;
keep all simulation equations, backend execution, and numerical validation in
`datasim`. Do not modify the public APIs or link interfaces of
`datasim`, `datasim_component`, `datasim_vertical`, `datasim_intake`, or
`datasim_pdf`. Keep manual ideal-RC execution separate from `datasim_vertical`.

The UI exposes each existing `RcConfig` field with the exact field name and SI
unit. It accepts only finite, fully consumed ASCII decimal/scientific notation
with `.` decimal point. The application parses values field-for-field and calls
`check_config()` before allocating a path or starting backend execution. The UI
does not convert units, clamp values, or reimplement supported-envelope rules.

## Thread ownership and lifecycle

An application-owned `SimulationWorker` receives copies of the validated
`RcConfig`, executable string, and output path, then moves to a `QThread`. The
worker alone calls `execute_rc_run()` in the production UI flow. Success/failure
signals are queued to the main window; QWidget updates happen in those GUI-thread
slots. Run and CSV-open controls are disabled while running. There is no cancel
control. If the user closes the window during execution, `closeEvent()` ignores
that event and closes only after worker completion and thread shutdown. The
window remains a live receiver until shutdown; no detached threads are used.

The documented UI states are `Idle`, `Validating`, `Running`, `Succeeded`, and
`Failed`. `Succeeded` requires a returned `RunResult` with
`Validation::passed == true`. Starting a run clears prior plotted data. Worker
exceptions are surfaced as `Failed` with the output path and only the artifact
names that exist at that point.

## Output path policy

The user chooses an existing output parent. After successful core validation,
the application creates a candidate child name using a high-resolution
timestamp and process-local atomic sequence. The candidate must not exist, and
is returned without creating it. `execute_rc_run()` remains responsible for
exclusive directory creation. If a competing process creates the same path,
the core reports a collision; the application preserves all files and reports
the failure. It does not delete, reuse, overwrite, or automatically clean any
historical run. Open CSV remains a separate read-only source path.

## Result presentation

The existing `WaveformWidget` receives `RunResult::samples`. The GUI displays
returned sample count, `backend_seconds`, `max_absolute_error_V`, `rmse_V`, and
`tolerance_V`; it does not recalculate validation errors. It labels successful
output as `Manual Ideal-RC Simulation`, never as datasheet-approved or physically
verified. The Open CSV action continues to label files as manually loaded
read-only data rather than newly executed simulation output.

## Consequences and limitations

- Accepted: strict numeric entry, existing configuration gate, worker thread,
  safe output path selection, existing execution/validation API, and result
  inspection in the existing viewer.
- Rejected: a custom solver, new SPICE netlist rules, general topology editor,
  approval/model workflows, cancellation, generic JSON framework, and core API
  changes.
- Qt remains optional, external, and not vendored. The project has no adopted
  license; this decision does not settle source licensing or Qt binary
  redistribution obligations.
- No physical measurement, general model accuracy, or datasheet-derived
  parameter claim is made.
