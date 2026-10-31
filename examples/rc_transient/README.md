# RC transient example

This example uses manually configured ideal components: R=1 kohm, C=1 uF and a
0-to-5 V pulse delayed by 1 ms. The spike simulates until 6 ms, exports adaptive
samples, and compares against an independent ideal-step RC formula.

Run the commands in [Phase 1 implementation](../../docs/PHASE_1_IMPLEMENTATION.md)
with a fresh output directory. The [validation report](../../docs/PHASE_1_VALIDATION.md)
defines the finite-rise exclusion, tolerance, numerical evidence and limitations.

`reference-run/` contains the actual successful run captured on 2026-10-31. It is
review evidence, not a replacement for running ngspice in the integration test.
The backend log is named `backend-output.txt` because the existing `.gitignore`
excludes `.log` files. No copyrighted datasheet, vendor model or simulator binary
is included.
