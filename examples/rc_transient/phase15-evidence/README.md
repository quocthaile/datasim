# Phase 1.5 evidence

Actual Windows/ngspice-47 runs, 2026-10-31. `study.json` summarizes four timestep
refinements and two baseline repeats. Each run has configuration/build metadata,
validation/checkpoint JSON, generated netlist and `backend-output.txt` (the captured
ngspice log, renamed to avoid the existing `.log` ignore rule).

Runs 0-3 include full common-grid error series. Run 0 includes the baseline
waveform; repeat waveforms were independently confirmed byte-identical. Full raw
tables and repeat CSVs remain locally in `build/phase15-study` and are regenerated
by `rc_study`. Tests never consume these files as substitute simulator output.

See [validation](../../../docs/PHASE_1_VALIDATION.md) and
[reproducibility](../../../docs/PHASE_1_REPRODUCIBILITY.md).
