# ADR-0011: Desktop visualization foundation

**Status:** Accepted for the Phase 5A prototype; owner licensing and redistribution decisions remain open.
**Date:** 2026-11-06

## Context and current architecture

At the Phase 4J baseline (`3fb583a0eb81640de421bc9049db9c0b80102e50`), CMake
defines five production libraries: `datasim` and `datasim_component` are base
targets; `datasim_vertical` publicly links `datasim_component` and `datasim`;
`datasim_intake` publicly links only `datasim_component`; `datasim_pdf` publicly
links only `datasim_intake`. Existing executables (`rc_demo`, `rc_study`,
`datasheet_extract`, and `datasheet_eval_dump`) are separate application targets.
There is no desktop application target.

`datasim::Sample` contains `time_s`, `vin_V`, and `vout_V` as `double` values;
`datasim::Samples` is `std::vector<Sample>`. `datasim::read_csv(std::istream&)`
requires the exact header `time_s,vin_V,vout_V`, parses classic-locale numbers,
then calls `check_samples()`. That validation rejects empty input, non-finite
values, negative time, and non-increasing time. `read_csv()` does not take a path;
the application boundary opens a local stream and calls it. `write_csv()` exists
but the visualization path does not call it. Thus the existing parser remains the
single CSV implementation and the viewer has no write path to the source file.

## Decision

Use **Qt 6 Widgets** for an optional Windows desktop viewer, with a small custom
`QWidget` waveform plot painted using `QPainter`. Use normal Widgets controls for
file selection, status, and the optional input-voltage overlay. Keep Qt discovery
and all Qt linkage under `DATASIM_BUILD_DESKTOP=ON`, default `OFF`. The application
links to `datasim`; none of the five production library link interfaces changes.
Qt PDF is evaluated as a useful later read-only preview, but it is not included in
the Phase 5A prototype to keep the first build dependency path smaller. Qt Charts,
Qt Graphs, and all GPL-restricted modules are excluded from this decision.

The display is a view of a validated `datasim::Samples` value. It does not expose
or invoke component review, model selection, `ExecutionPolicy`, simulation
configuration, or result writing APIs. The data flow is:

```text
local results.csv -> std::ifstream (read-only) -> datasim::read_csv()
  -> datasim::Samples -> viewer read-only adapter -> waveform QWidget
```

## Alternatives considered

| Criterion | Qt 6 Widgets + custom waveform / optional Qt PDF | Dear ImGui + Win32/DX11 backend |
| --- | --- | --- |
| Windows x64 MSYS2 UCRT64 | MSYS2 publishes UCRT64 Qt 6 packages; exact environment build is still required. | Dear ImGui includes Win32 and DirectX backends; an MSYS2 UCRT64 integration must still be built and maintained. |
| C++20 / CMake | Qt provides CMake package targets; selected `Qt6::Widgets` is target-scoped. | CMake integration is straightforward, but the project must compose core sources and backend sources and own backend versioning. |
| External dependencies | Qt Widgets entails a larger shared-library/runtime installation. Qt PDF would add another module and is deferred. | Dear ImGui core is self-contained; Win32/DX11 keeps the toolkit dependency count low, but graphics/platform backend integration remains application code. |
| Waveform suitability | A custom `QPainter` widget is adequate for a bounded static 2D trace and allows axis/units to be explicit. No plotting module is needed. | Draw lists suit fast engineering/debug visualization, but axes, labels, layout, and plot interactions are all application-owned. |
| PDF viewing | Qt PDF offers `QPdfDocument` and `QPdfView`; not enabled in 5A. | Separate PDF rendering/viewer integration is required. |
| Tables, forms, keyboard, accessibility | Widgets provide standard desktop controls and keyboard navigation; Qt documents `QAccessible` integration. A custom plot still needs accessibility consideration. | Dear ImGui's own documentation identifies it as visualization/debug tooling and states accessibility features are not supported; less suitable for the file/forms workflow. |
| Thread/event loop | Qt's GUI event loop and widget update model fit a single-threaded local file reader; CSV parsing is synchronous and small. | Application must own the Win32 message pump and frame/render loop. |
| Packaging/deployment | Qt shared libraries/plugins need deployment and version discipline; no binaries are bundled by this change. | Win32/DX11 backend reduces framework runtime packaging, but graphics driver and runtime behavior remain deployment concerns. |
| Binary/license obligations | Qt open-source licensing includes LGPLv3 for many modules and GPL-only modules; module-specific review is needed. | Dear ImGui is MIT; its selected platform/renderer backend and any added dependencies still require notices and review. |
| Testability/maintenance | The path adapter can be tested without a GUI; plot inspection remains a separate UI verification. Qt is a broad maintained desktop framework. | Core display transforms can be unit tested, but the custom platform/render loop adds maintenance and visual test work. |

Qt was selected because this milestone is a conventional file-open desktop
workflow with labels, status, keyboard interaction, and a read-only engineering
plot. The Qt documentation describes Widgets as mature desktop UI components and
documents keyboard navigation and accessibility interfaces. Dear ImGui's primary
documentation explicitly scopes it toward programmer/debug tools and says it does
not provide accessibility features, which weighs against a general desktop file
viewer. Dear ImGui remains a viable future option if the product scope shifts to an
embedded diagnostic/debug panel.

## Compatibility and licensing evidence

- MSYS2 publishes Qt 6 packages for UCRT64. The local prototype environment
  installed Qt 6.11.1-1 `qt6-base` and `qt6-pdf` packages with MSYS2 `pacman`;
  exact compiler and build results are recorded in
  [Phase 5A validation](../PHASE_5A_VALIDATION.md).
- Qt's licensing page describes commercial, LGPLv3, and GPLv3 terms and identifies
  GPL-only modules. The project has no adopted license. Neither this ADR nor use of
  a system package selects a project license or accepts redistribution terms.
- Dear ImGui's upstream repository identifies its license as MIT and documents
  `imgui_impl_win32` and `imgui_impl_dx11`; its accessibility limitation is in the
  upstream documentation.
- Qt Charts/Graphs are not used. Qt Graphs is listed by Qt among GPLv3 modules;
  no GPL-restricted module is part of the selected target.

The owner must decide the repository license and any distribution model before
shipping a Qt-linked binary or bundling Qt. This source-only prototype is not a
redistribution-rights determination. No Qt or third-party binary, font, or vendor
asset is added to the repository.

## Consequences and rejected options

- **Accepted:** optional `datasim_viewer`, Qt6 Widgets, custom `QPainter` plot,
  existing CSV parser, read-only adapter, and isolated sample-loader tests.
- **Rejected for 5A:** Dear ImGui + Win32/DX11. It is technically capable and
  permissively licensed, but its documented accessibility gap and added custom
  desktop workflow implementation make it a weaker match for this viewer.
- **Deferred:** Qt PDF preview. It is technically available, but the synthetic PDF
  preview is optional and does not improve waveform verification; defer until a
  separately scoped review-only document workflow is requested.
- **Prohibited by this decision:** Qt Charts, Qt Graphs, GPL-only Qt modules, core
  library dependency changes, and automatic result or engineering-state edits.

## References

1. [Qt Group, Qt Licensing](https://doc.qt.io/qt-6/licensing.html) (`qt6Licensing`),
   including the open-source licensing choices and GPLv3 module list.
2. [Qt Group, User Interfaces](https://doc.qt.io/qt-6/topics-ui.html) (`qt6Widgets`),
   including “Qt Widgets User Interfaces”.
3. [Qt Group, Accessibility for QWidget Applications](https://doc.qt.io/qt-6/accessible-qwidget.html)
   (`qt6WidgetAccessibility`).
4. [Qt Group, Qt PDF](https://doc.qt.io/qt-6/qtpdf-index.html) (`qt6Pdf`),
   describing `QPdfDocument`, `QPdfPageRenderer`, and `QPdfView`.
5. [MSYS2, UCRT64 Qt 6 package group](https://packages.msys2.org/groups/mingw-w64-ucrt-x86_64-qt6)
   and [Qt PDF package](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-qt6-pdf)
   (`msys2UcrtQt6`).
6. [Omar Cornut, Dear ImGui backends](https://github.com/ocornut/imgui/blob/master/docs/BACKENDS.md)
   (`dearImGuiBackends`).
7. [Omar Cornut, Dear ImGui documentation overview](https://github.com/ocornut/imgui/blob/master/docs/README.md)
   (`dearImGuiOverview`), covering its tools/debug visualization scope and accessibility limitation.
8. [Omar Cornut, Dear ImGui MIT License](https://github.com/ocornut/imgui/blob/master/LICENSE.txt)
   (`dearImGuiLicense`).
