# Phase 5A plan: desktop visualization foundation

**Status:** IMPLEMENTED in this review branch; verification is recorded separately.

## Objective

Add an optional, read-only Windows desktop viewer for existing ideal-RC
`results.csv` artifacts. This is a visualization foundation, not a simulation
editor, model selector, review tool, or physical measurement instrument.

## Scope

- Add the Qt 6 Widgets decision and retain unresolved licensing/redistribution
  decisions for the repository owner.
- Add `datasim_viewer` behind `DATASIM_BUILD_DESKTOP=OFF` by default.
- Open a user-selected local CSV using the existing `datasim::read_csv()` and
  `datasim::Samples` contract.
- Render output voltage against time with labeled seconds/volts axes, legend, and
  loaded-file/sample-range information. Allow optional overlay of input voltage.
- Show an explicit load error in the plot and status area.
- Test valid, invalid/non-finite, missing-file, and unchanged-input cases through
  the read-only loading adapter.

## Existing architecture boundary

The Phase 4J dependency graph is preserved:

```text
datasim_vertical -> datasim_component + datasim
datasim_intake   -> datasim_component
datasim_pdf      -> datasim_intake
datasim          (independent)
datasim_component (independent)
```

The viewer is a separate application target linked to `datasim` and `Qt6::Widgets`.
No existing production library public API or link interface changes.

## Exclusions

No writing or rewriting CSV, simulation launch/configuration, engineering review,
approval, model loading/selection, general JSON layer, OCR, PDF table selection,
or physical accuracy claims. Qt PDF rendering remains deferred although the
module is available in the tested package environment.

## Exit criteria

1. ADR-0011 records the existing architecture, toolkit comparison, decision, and
   license boundaries.
2. Default CMake configuration does not discover Qt.
3. Optional Qt build compiles without changing the five library interfaces.
4. The path adapter delegates parsing/validation to `datasim::read_csv()` and
   rejects unreadable, malformed, or non-finite data visibly.
5. CTest release gates, real ngspice integration, and convergence/repeatability
   remain green in the environment used for the validation report.
6. GUI launch and visual inspection are reported separately from compilation and
   automated loading tests.

## Follow-on

Potential Phase 5B work may evaluate evidence/provenance panels and an optional
read-only synthetic PDF preview, each with explicit coordinate-space boundaries.
These are proposed only and are not implemented in Phase 5A.
