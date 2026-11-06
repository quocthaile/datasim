# Phase 5B plan: interactive ideal-RC workbench

**Status:** IMPLEMENTED on `feat/phase-5b-rc-simulation-workbench`; verification is recorded in [validation](PHASE_5B_VALIDATION.md).

## Objective

Extend the Phase 5A optional `datasim_viewer` with manual ideal-RC configuration,
authoritative configuration validation through `datasim::check_config()`,
background execution through the existing `datasim::execute_rc_run()`, and
read-only display of returned samples, metrics, and run artifacts.

## User workflow

1. Configure R, C, source voltage, timing, step, and duration in the exact units
   shown by the existing `datasim::RcConfig` field names.
2. Enter finite decimal or scientific notation with `.` as the decimal point;
   omit units, whitespace, and grouping separators.
3. Choose an existing output parent and ngspice executable.
4. Run. Invalid input is rejected before a run directory is created or ngspice is
   started.
5. On success, inspect Vout, optional Vin, returned validation metrics, timing,
   and the exact directory/artifact list. All run directories are retained.
6. Use Open CSV to inspect an existing result separately; this remains a
   read-only view and is labeled as a manually loaded CSV.

## Supported configuration envelope

All eleven `RcConfig` fields are exposed. Defaults come from `RcConfig{}`.
`parse_rc_config()` accepts strict locale-independent decimal/scientific values,
then calls the unchanged `datasim::check_config()`. That function remains the
authority for positive finite values, zero initial voltage, pulse timing, rise
time and step constraints relative to RC, charging-only duration, and the maximum
nominal step count. The application does not clamp values, convert units, or
weaken these constraints. `tau` appears only after the configuration passes.

## Exclusions

No arbitrary schematic/netlist editing, component knowledge, model selection,
`datasim_vertical` execution, CSV rewriting, cancellation, persistence, generic
JSON parser, or physical measurement claim is added.
