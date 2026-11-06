# Application assets

`branding/datasim-mark.svg` is the original project-owned monochrome Datasim mark introduced in Phase 5C. Its resource path remains `:/datasim/branding/datasim-mark.svg`.

Phase 5D adds six newly authored small SVG icons for the evidence review UI. No external artwork, image package, binary font or manufacturer PDF has been copied. The final repository license and distribution policy remain an owner decision.

| File | Qt resource URL | Intended meaning |
| --- | --- | --- |
| `icons/review-candidate.svg` | `:/datasim/icons/review-candidate.svg` | `ReviewState::candidate` |
| `icons/review-needs-review.svg` | `:/datasim/icons/review-needs-review.svg` | `ReviewState::needs_review` |
| `icons/review-approved.svg` | `:/datasim/icons/review-approved.svg` | `ReviewState::approved` |
| `icons/review-rejected.svg` | `:/datasim/icons/review-rejected.svg` | `ReviewState::rejected` |
| `icons/review-superseded.svg` | `:/datasim/icons/review-superseded.svg` | `ReviewState::superseded` |
| `icons/evidence-location.svg` | `:/datasim/icons/evidence-location.svg` | Source evidence locator |

All icons are 24x24 vector line drawings using the existing branding navy (`#12304A`), with distinct shapes rather than color as the only state signal. Always render explicit status labels and accessible tooltips; icons are presentation only, **never the source of review state or authority**. Use actual `ReviewState` values and the domain review lifecycle as the authoritative source.

The small SVG files are compiled by Qt Resource System with `assets/viewer.qrc` when `DATASIM_BUILD_PDF_WORKBENCH=ON`. Check runtime SVG support in the chosen Qt distribution. Changes to these committed assets are expected to change the documented source/build fingerprint. User datasheets remain local external files; synthetic PDFs remain under `tests/fixtures/`.
