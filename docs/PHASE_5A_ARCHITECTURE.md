# Phase 5A architecture: read-only results viewer

## Existing system

At baseline `3fb583a0eb81640de421bc9049db9c0b80102e50`, CMake has five production
libraries with this link graph:

```text
datasim_vertical -> datasim_component + datasim
datasim_intake   -> datasim_component
datasim_pdf      -> datasim_intake
datasim          (independent)
datasim_component (independent)
```

The existing application executables are `rc_demo`, `rc_study`,
`datasheet_extract`, and `datasheet_eval_dump`. There was no desktop application
target. The architecture contracts and existing target definitions are recorded
in the [Phase 4 final validation report](PHASE_4_FINAL_VALIDATION.md).

`datasim::Sample` is `{double time_s, double vin_V, double vout_V}` and
`datasim::Samples` is `std::vector<Sample>`. `datasim::read_csv(std::istream&)`
requires exactly `time_s,vin_V,vout_V`, reads classic-locale numeric cells, and
calls `check_samples()`. `check_samples()` rejects empty collections, non-finite
values, negative times, and non-increasing times. The viewer opens an `ifstream`
in read-only mode and delegates all CSV parsing to this function. The application
does not call `write_csv()`.

## Added application layer

```mermaid
flowchart LR
  F[Local results.csv] --> R[std::ifstream, read only]
  R --> P[datasim::read_csv]
  P --> S[datasim::Samples]
  S --> A[Read-only samples adapter]
  A --> V[Qt Widgets waveform view]
  V -->|optional overlay| I[Vin series]
```

The adapter is `apps/datasim_viewer/read_only_samples.*`. It has no dependency on
Qt, opens the path read-only, and returns `datasim::Samples` plus the source path.
Open and parse failures become exceptions with user-visible diagnostics. The
window catches them, clears the prior plotted data, and displays an error state.

`WaveformWidget` maps time linearly to the horizontal plot coordinate and voltage
linearly to the vertical plot coordinate. Axes are explicitly labeled `Time (s)`
and `Voltage (V)`. `Vout` is always displayed; a checkbox overlays `Vin` on the
same voltage axis. This plot is a direct display of parser-validated samples; it
does not transform units, smooth data, interpolate points, or report measurement
accuracy. Input remains unchanged.

## Target and dependency isolation

`DATASIM_BUILD_DESKTOP` defaults to `OFF`. Only when enabled does CMake run
`find_package(Qt6 6.11 REQUIRED COMPONENTS Widgets)` and add `datasim_viewer`.
The executable links privately to `datasim` and `Qt6::Widgets`; no GUI dependency
is added to `datasim`, `datasim_component`, `datasim_vertical`, `datasim_intake`,
or `datasim_pdf`. The custom waveform is implemented with `QPainter`; neither Qt
Charts nor Qt Graphs is used. PDF preview was optional and is deferred.

## Trust and behavior boundaries

- Viewing a CSV grants no ability to approve candidate observations or promote
  component knowledge.
- The viewer does not select a model or invoke `ExecutionPolicy`.
- The viewer cannot launch simulation or change circuit configuration.
- The viewer does not write or silently rewrite loaded results.
- The displayed waveform is simulation output. It is not physical measurement or
  a claim that the model matches a real component.
- This path contains no JSON serialization or PDF coordinate conversion.

## Licensing and deployment boundary

The toolkit comparison and decision are in
[ADR-0011](decisions/ADR-0011-desktop-visualization-foundation.md). Qt is an
external system dependency and is not vendored. The project has no adopted
license; this ADR does not settle project licensing or Qt binary redistribution.
Deployment packaging and owner license decisions remain open.
