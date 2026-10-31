# Phase 1.5 reproducibility

Status: VERIFIED local Windows numerical repeatability. Fresh-checkout and hosted
CI verification are tracked separately below; no multi-platform claim is made.

## Environment and commands

Windows x64, GCC 16.2.0 (MSYS2 UCRT Rev3), C++20, CMake 4.4.0-rc2, Ninja, Release;
ngspice-47 console executable from the hash-checked MSYS2 47-1 package. Actual RC
logs use SPARSE 1.3 despite the build's KLU support. See the
[runtime provisioning instructions](PHASE_1_IMPLEMENTATION.md) for the extracted
runtime and corrected native Windows spinit paths. Runtime packages stay under
ignored `build/tools`; no dependency was added to the source link requirements.

Executed from repository root in PowerShell:

```powershell
cmake -S . -B build/phase15 -G Ninja -DCMAKE_BUILD_TYPE=Release -DDATASIM_NGSPICE=D:/projects/datasim/build/tools/msys2-ngspice/ucrt64/bin/ngspice_con.exe
cmake --build build/phase15
ctest --test-dir build/phase15 --output-on-failure -V --output-junit tests.xml
./build/phase15/rc_study.exe D:/projects/datasim/build/tools/msys2-ngspice/ucrt64/bin/ngspice_con.exe build/phase15-study
```

Study output must be a new directory. It runs four refinements and two additional
baseline repeats; it never deletes previous artifacts. For one run use
`rc_demo --ngspice EXECUTABLE --output NEW_DIRECTORY` with the existing numeric
options. The executable path must have its matching runtime DLLs available.

## Artifact contract (schema version 1)

The original flat per-run structure is retained rather than adding nested folders:

```text
study.json                     # six-run metrics, durations, repeat comparisons
run-0/                         # and run-1 through run-5
  circuit.cir                  # generated input
  samples.dat                  # untouched backend table
  ngspice.log                  # combined stdout/stderr
  version.txt
  results.csv                  # time_s,vin_V,vout_V; no nondeterministic fields
  errors.csv                   # common-grid pointwise ideal-step errors
  run_metadata.json
  validation_report.json
```

Metadata records manually configured origin, all component/source/transient
parameters, solver settings, executable/version, execution/validation statuses,
runtime, error, application version, Git revision at CMake configuration, source
SHA256, compiler/build configuration and platform. FNV-1a-64 identifies the exact
generated netlist **normalized to LF**, independent of Windows CRLF text output;
it is not a cryptographic integrity guarantee. Source SHA256 combines sorted
source/header/test/CMake inputs, normalizing CRLF to LF, including uncommitted files.
Reconfigure/rebuild after changes; the captured study was built from a dirty working
tree based on `021513c`, explicitly recorded rather than falsely attributed to HEAD.

Validation JSON reports the exact interval, interpolation method, native and common
metrics, unchanged tolerance and characteristic checkpoints. Failed runs record
NOT_RUN/FAILED statuses and an error. No malformed or absent output is promoted.
Output directories are atomically created, explicit collisions fail, and helper
workspaces retry name collisions. Cleanup releases process/OS resources; diagnostic
and successful files are intentionally retained. No recursive file deletion occurs.

## Actual repeatability results

Runs 0, 4 and 5 used identical baseline configuration and environment. Compared
vin/vout on the same 4001-point grid with absolute tolerance **1e-12 V**, fixed in
the study implementation. Both repeated comparisons had **maximum delta 0 V**.
Additionally, an independent byte comparison found all three `results.csv` files
identical, with SHA256:

`9c923c4d86a96bdf42fe3bbd84a00cc6ae8519f5ac8ea3858b614c30260f26f9`.

Numerical reproducibility and byte identity are separate observations. Complete
artifact directories are not byte-identical: runtime and executable/build identity
metadata may differ. There is no promise of cross-platform bit identity. Source
identity for this study is
`7eb377c10b7150080a2c67211ba3016b88baf42865ec9fcc66938aa546877db3`.

## Evidence and verification levels

[Study JSON](../examples/rc_transient/phase15-evidence/study.json) includes all
configurations by run, metrics, sample counts and runtime observations. Each run's
metadata contains the full configuration. Captured per-run reports/logs and all
four refinement error series are in `examples/rc_transient/phase15-evidence/`.
The baseline waveform is also saved; all raw/full repeat artifacts remain under
`build/phase15-study/`. Captured log files use `backend-output.txt` to respect the
existing `.log` ignore rule. [CTest log](../examples/rc_transient/phase15-evidence/tests-output.txt)
and [JUnit results](../examples/rc_transient/phase15-evidence/tests.xml) record the
actual local tests. These files are evidence, never canned integration-test inputs.

The Windows GitHub Actions workflow installs the exact ngspice 47-1 package after
checking its SHA256, checks the reported version, fixes package relocation paths,
builds a clean checkout, runs unit/process tests and required real integration,
runs the study, and collects logs/reports even on failure. Toolchain/dependency
package versions can evolve; they are recorded at execution rather than claimed
fully locked. Hosted CI is IMPLEMENTED, not yet executed or VERIFIED.

Fresh-checkout verification is pending at this documentation snapshot. It must
run before declaring the local Phase 1.5 exit complete. A clean build on the same
installed Windows toolchain does not constitute a fresh-OS or Linux verification.
