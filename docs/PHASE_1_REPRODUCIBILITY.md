# Phase 1.5 reproducibility

Status: VERIFIED local Windows numerical repeatability, fresh-checkout build and
hosted Windows CI. Hosted evidence is recorded separately below; no multi-platform
claim is made.

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
fully locked. Hosted CI is now VERIFIED by the completed run documented below.

## Fresh-checkout verification completed

Cloned implementation commit `52e562a` into `build/phase15-clean-checkout` with
`git clone --no-hardlinks --branch codex/phase-1-5-hardening D:/projects/datasim D:/projects/datasim/build/phase15-clean-checkout`.
Confirmed an empty Git status before and after the test run. No prior build files
were copied. The exact commands in that new checkout were:

```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;C:\Program Files\CMake\bin;C:\Program Files\Git\cmd;C:\Windows\System32;C:\Windows'
cmake -S . -B build/clean -G Ninja -DCMAKE_BUILD_TYPE=Release -DDATASIM_NGSPICE=D:/projects/datasim/build/tools/msys2-ngspice/ucrt64/bin/ngspice_con.exe
cmake --build build/clean
ctest --test-dir build/clean --output-on-failure -V --output-junit clean-tests.xml
./build/clean/rc_study.exe D:/projects/datasim/build/tools/msys2-ngspice/ucrt64/bin/ngspice_con.exe build/clean-study
```

Clean configure/build succeeded; **3 tests passed, 0 failed, 0 skipped**, including
92 unit checks. All six clean-study CSVs were byte-identical to the original study,
and the build's normalized source SHA256 matched. Later changes cover
documentation/evidence, CSV checkout attributes and CI provisioning, not C++
executable source.
See [clean verification JSON](../examples/rc_transient/phase15-evidence/clean-verification.json),
[clean test log](../examples/rc_transient/phase15-evidence/clean-tests-output.txt),
[clean study](../examples/rc_transient/phase15-evidence/clean-study.json).

The clean checkout also exposed Git's Windows CRLF conversion of captured CSVs.
Targeted `.gitattributes` now preserves LF for Phase 1.5 evidence CSVs. This does
not change generated waveforms or numerical values. The original run SHA256 above
refers to the LF waveform bytes.

The local Phase 1.5 exit criteria are satisfied for this Windows environment.
This verifies a fresh checkout/build with explicitly selected existing toolchain
and runtime dependencies, not a fresh OS installation or Linux/MSVC support.
At that point hosted Actions execution remained unverified. The subsequent hosted
repair and verification are recorded below. No Phase 2 work is authorized by this result.

## Hosted CI failure and repair (2026-10-31)

The release-gate task started at `b88ddec6d4f5509c59546d7ff1bb01385a969bc7`
on `codex/phase-1-5-hardening`, with a clean working tree.
[Failed run 41216041196](https://github.com/quocthaile/datasim/actions/runs/41216041196)
built successfully and passed unit/process tests, but real integration failed;
the convergence step did not run. Its archived backend log contains duplicated
paths such as `D:/a/_temp/msys64D:/a/_temp/msys64/ucrt64/lib/ngspice/analog.cm`.
The [failure record](../examples/rc_transient/hosted-ci/failed-41216041196/failure.json)
and [original backend output](../examples/rc_transient/hosted-ci/failed-41216041196/backend-output.txt)
are preserved separately from all historical local evidence.

The pinned archive contains `/ucrt64/...` paths, but its `.INSTALL` `post_install`
hook already rewrites them with `cygpath -m`. The workflow's subsequent global
`sed` replacement matched `/ucrt64/` inside those valid native paths and duplicated
the drive-qualified prefix. The installed format was confirmed from the repaired
runner's captured `spinit.before`, not inferred from the raw archive alone.

The workflow now passes a dynamic `cygpath -m /ucrt64` prefix to
[`configure_ngspice.py`](../scripts/ci/configure_ngspice.py). It converts only
`codemodel`/`osdi` argument tokens beginning `/ucrt64/`, preserves native absolute
tokens, comments, quoting and conditionals, and rejects unknown/corrupted paths.
Five tests cover raw, native, mixed/quoted paths, repeated conversion and the
actual duplicated-prefix regression. Before/after files and path reports are
uploaded. The helper checks all seven code-model files and starts actual ngspice
with installed initialization enabled; exit errors, warnings and failed startup
remain fatal. The workflow invokes it twice and verifies unchanged final hashes.
The C++ diagnostic gate, model, independent reference and 0.05 V tolerance were
not modified.

## Hosted verification completed

[Successful run 41217248663](https://github.com/quocthaile/datasim/actions/runs/41217248663)
completed successfully at repair commit
`86d6666677b98bef8ac4959b7f25fa9bd15cbe9f` on 2026-10-31 (UTC).
The full [rc-verification-windows artifact](https://github.com/quocthaile/datasim/actions/runs/41217248663/artifacts/13048542135)
was downloaded and audited after completion. It includes raw samples, all six
waveform/error CSVs, netlists, logs, JSON reports, fault artifacts, JUnit output,
the packaged install script and runtime configuration before/after both checks.
GitHub artifact availability is subject to repository retention settings.

| Item | Observed hosted result |
| --- | --- |
| Runner | Windows Server 2025 Datacenter 10.0.26100, AMD64; Actions runner 2.338.2 |
| Image | `windows-2025-vs2026`, version `20261021.278.1` |
| Compiler/build | GNU 16.2.0, MSYS2 UCRT package 16.2.0-6; C++20 Release; CMake 4.4.5-1; Ninja 1.13.2-1 |
| Backend | Hash-checked ngspice 47-1; reported ngspice-47; actual RC solver SPARSE 1.3 |
| Package/runtime checks | PASS; five normalization tests; seven code models checked on each of two real startup invocations; zero unexpected diagnostics |
| Installed paths | Native Windows paths already correct; zero conversions, before/after hashes equal on both invocations |
| Configure/build | PASS from Actions checkout |
| CTest | 3 passed, 0 failed, 0 skipped: unit (92 checks), backend_errors, ngspice_rc |
| Real integration | PASS; 640 samples; intentionally invalid netlist separately rejected with exit 1 |
| Study | PASS; four refinements and two additional baseline repeats; 4001-point grid and 0.05 V tolerance retained |
| Repeats | Maximum numerical delta 0 V at 1e-12 V repeat tolerance; baseline CSV bytes identical |

The [audit record](../examples/rc_transient/hosted-ci/passed-41217248663/audit.json)
contains run/head identity, step outcomes, artifact and CSV hashes, independent
metric recomputation and environment versions. [Study JSON](../examples/rc_transient/hosted-ci/passed-41217248663/study.json),
[unit/process JUnit](../examples/rc_transient/hosted-ci/passed-41217248663/unit.xml),
[integration JUnit](../examples/rc_transient/hosted-ci/passed-41217248663/integration.xml),
and all six reports/netlists/logs are retained next to it. Runtime snapshots are
under `runtime-config/`; captured `.log` files use `.txt` in this evidence copy.
Full downloaded artifacts remain locally under `build/hosted-ci/passed-41217248663/`.

Hosted and local configurations, normalized netlists, validation report objects
and study numerical fields match exactly. All six hosted waveform CSVs are
byte-identical to their original local run files. Independent Python CSV/math
recomputation agrees with reported metrics to floating-point rounding (RMSE
differences below 4e-19 V). All 30 characteristic checkpoints pass. The recorded
source SHA256 is the same `7eb377c...77db3` cited above.

Environment differences include a hosted Windows Server image, package-managed
runtime rather than local extraction, GCC package revision 6 rather than 3,
CMake 4.4.5 rather than 4.4.0-rc2, executable paths and run durations. None produced
a numerical or waveform-byte difference in this experiment; durations are not
controlled benchmarks. Hosted run metadata records `git_revision_at_configure`
as `source-export` because Git revision discovery fell back in that build
environment. The Actions head SHA and matching normalized C++ source hash provide
the verified revision association; the metadata field is preserved as generated.
It does not hash the Python helper or workflow; those are identified by the
Actions commit. Cross-platform identity and fully locked transitive dependencies
remain unverified. No Phase 2 work has started.
