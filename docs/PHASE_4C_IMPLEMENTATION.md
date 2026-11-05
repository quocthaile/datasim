# Phase 4C implementation: deterministic PDF table extraction

Status: **IMPLEMENTED and VERIFIED** on `main`, merged via PR #7 at
`1704f726d901f3e1d00f9cdc918e80d352b443ac` from the Phase 4B `main` baseline
(`7d9af1c88929fe3c29d55e1c2438e8d2041acb2a`).
Hosted Windows CI run [42000652152](https://github.com/quocthaile/datasim/actions/runs/42000652152)
(PR #7 head `8291401`) and post-merge `main` run
[42017272043](https://github.com/quocthaile/datasim/actions/runs/42017272043)
passed on synthetic fixtures. See
[validation](PHASE_4C_VALIDATION.md) for local and hosted results. No
real-datasheet accuracy or physical measurement is claimed.

## Adapter boundary

`datasim_pdf` is a C++20 adapter that links only to `datasim_intake`. It launches
local Poppler `pdfinfo` and `pdftotext -bbox-layout` executables through a
shell-free process boundary. `datasim_intake` itself remains independent of PDF
processing, persistence, simulation and external services.

The public API is in
[`table_extraction.hpp`](../src/datasim/pdf/table_extraction.hpp). A request
contains the local PDF/tool paths, zero-based PDF page index, manually supplied
printed label, component identity, source-document metadata/rights, curator,
parameter category, table region and explicit horizontal spans for parameter,
unit, min, typ, max, and optional condition cells.

The algorithm obtains the real page count from `pdfinfo`; selects one page with
`pdftotext -bbox-layout`; filters words to the selected rectangle; groups words
by explicit vertical tolerance; and maps each word only to one caller-supplied
column span. Any word that crosses or falls outside spans makes the extraction
fail closed. Rows without a parameter name are omitted with a diagnostic.
Non-empty min/typ/max cells become distinct observations; empty and dash cells
stay absent. Numeric parsing uses Phase 4B's strict scalar parser and leaves
ambiguous/footnote-marked strings unparsed.

The adapter creates one evidence location for the parameter/unit cells and one
for the value cell, both linked from each observation. It preserves zero-based
PDF index separately from the explicit printed label, records table/row/column
location, and retains `*` footnote markers as evidence references. The narrow
condition grammar is one or more semicolon-separated `NAME=NUMBER UNIT` entries;
other condition text stays in the intake sidecar with a diagnostic. A blank unit
stays unknown and suppresses numeric parsing; it is not inferred to mean unitless.
Publisher, exact MPN, category, rights, page label,
coordinates and table selection are supplied by the caller. They are never
derived from PDF page index or guessed from names.

Observation IDs are stable strings derived from source-document ID, table ID,
page index, row order and qualifier column. The adapter uses no timestamps or
random values for result IDs. It sets `ExtractionMethod::deterministic_table_parser`
and the backend version. Every result goes through `validate_intake_manifest()`
and `build_candidate_bundle()`; resulting observations remain
`ReviewState::candidate` and no knowledge/model records are created.

## Runnable demonstration

Generate the fictional two-page fixture and run from an MSYS2 UCRT shell with
Poppler on `PATH`:

```text
python tests/fixtures/make_synthetic_datasheet_pdf.py
cmake -S . -B build/phase4c -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build/phase4c -j 4
build/phase4c/datasheet_extract.exe \
  --pdf tests/fixtures/synthetic_datasheet.pdf --page 0 --printed-page 5 \
  --table-id TABLE_R --manufacturer "Datasim Synthetic" --publisher "Datasim Synthetic" \
  --mpn SIM-R-10K \
  --family resistor --component-id cmp.demo.resistor --document-id doc.demo.resistor \
  --title "Fictional electrical characteristics" --revision fixture-rev-1 \
  --rights-note "Generated fictional PDF fixture; no vendor material." \
  --redistribution-permitted false --category electrical \
  --region 20,80,690,215 --header 80,110 --parameter 20,215 --unit 215,285 \
  --min 285,350 --typ 350,420 --max 420,490 --condition 490,690
```

Replace the example identity, rights declaration, page label and layout with
values intentionally supplied by the operator for the local document. The
program prints candidate values, source-reference counts and diagnostics; it
does not write a generic manifest or approve data.

## Supported subset and failure modes

Supported: selectable text on one selected page; a single rectangular table
region; stable horizontal rows; non-overlapping, explicitly configured columns;
ordinary min/typ/max cells; optional condition text in the documented grammar.

Not supported: OCR or image-only pages; arbitrary table detection; merged cells;
multi-page continuation; rotated or vertically written text; overlapping or
irregular columns; locale numeric notation; value ranges/tolerances/inequalities;
automatic unit conversion; full footnote prose interpretation; automatic MPN or
printed-page recovery. Unsupported geometry or missing selectable text yields no
candidate bundle. Ambiguous values remain raw text with diagnostics. The tool
does not establish source authenticity, licensing rights or extraction accuracy
on real vendor datasheets.
