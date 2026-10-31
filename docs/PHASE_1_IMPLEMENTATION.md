# Phase 1 implementation

Status: IMPLEMENTED and VERIFIED for the Windows/GCC/ngspice configuration below.
This is an ideal first-order RC feasibility spike, not a general circuit simulator.

## Initial state and scope

The starting checkout was clean on `docs/project-foundation`, tracking its origin
branch, at `b319636` (foundation documentation) after `e48c97d` (initial commit).
There was no source code, build system, test framework or license file. Root
`AGENTS.md` was missing; guidance existed at `docs/AGENTS.md`. All other requested
architecture documents were present and read. Work is on `feat/phase-1-ngspice-rc`.
Root guidance is now added and the existing documentation updated for Phase 1.

The user authorized C++20 and an initial CMake build. This supersedes the historical
documentation-only task restriction; [ADR-0002](decisions/ADR-0002-phase-1-ngspice-executable.md)
records the conflict and decision. Existing `.gitignore` is unchanged. No Phase 2,
GUI, OCR, LLM, database, custom solver, or semiconductor-model work is included.

## Source organization and execution

| Location | Responsibility |
| --- | --- |
| `src/datasim/simulation/circuit/` | Manual ideal RC/source/transient configuration and supported-range checks. |
| `src/datasim/simulation/backend/` | ngspice netlist, version query, shell-free process launch, diagnostics and strict table reader. |
| `src/datasim/simulation/io/` | Backend-independent samples, strict CSV reader/writer, finite values and increasing time checks. |
| `src/datasim/simulation/analysis/` | Independent closed-form RC voltage using only configuration and time. |
| `src/datasim/simulation/validation/` | Source/time coverage checks and pointwise/max/RMSE/normalized errors. |
| `apps/rc_demo/` | CLI composition, fresh run directory, metadata, output and failure exit status. |
| `tests/unit/` | Mathematical/configuration/parser tests and separate fault-injection subprocess fixture. |
| `tests/integration/` | Actual ngspice execution, CSV roundtrip, numerical tolerance and stale-artifact guard. |
| `examples/rc_transient/reference-run/` | Captured real run: netlist, normalized CSV, error CSV, metadata and backend log. |

Execution is configuration check -> fresh directory -> executable lookup/version
query -> netlist -> `ngspice_con.exe -n -b circuit.cir` -> exit/log checks ->
strict numerical table retrieval -> CSV -> independent reference validation ->
error CSV and final metadata. On other systems the executable is usually named
`ngspice`. No shell interprets paths or arguments. Windows uses `CreateProcessW`
without a visible window; POSIX uses fork/exec. Processes have a 30-second limit.
Only the Windows path has been exercised here.

The narrow adapter returns generic samples, keeping ngspice syntax out of analysis
and validation. There is no plugin registry or general circuit schema. Unknown CLI
options/configurations fail. The CLI supports `--r-ohm`, `--c-f`, `--voltage-v`,
`--step-s`, `--duration-s`; other source fields are explicit in `RcConfig`.
The executable must be a working console ngspice build with its runtime libraries.

## Build and run

Requirements: C++20 compiler, CMake >= 3.20, a build tool, and an external ngspice
executable for simulation. No ngspice headers/link libraries or test framework
are required to build. The tested host is Windows x64, MSYS2 UCRT GCC 16.2.0
(Rev3), CMake 4.4.0-rc2, Ninja, Release configuration.

These are the exact successful commands from the repository root in PowerShell:

```powershell
cmake -S . -B build/phase1 -G Ninja -DCMAKE_BUILD_TYPE=Release -DDATASIM_NGSPICE=D:/projects/datasim/build/tools/msys2-ngspice/ucrt64/bin/ngspice_con.exe
cmake --build build/phase1
ctest --test-dir build/phase1 --output-on-failure -V
./build/phase1/rc_demo.exe --ngspice D:/projects/datasim/build/tools/msys2-ngspice/ucrt64/bin/ngspice_con.exe --output build/rc-final
```

`build/rc-final` now exists: use a new output directory on subsequent runs. Existing
directories are deliberately rejected. Configure `DATASIM_NGSPICE` to your own
console executable path. This CMake setting controls the integration test; the
demo independently uses `--ngspice` or PATH. Keep the compiler/runtime DLL directory
on PATH; this host already has `C:\msys64\ucrt64\bin` there.

Portable command shape (POSIX implementation not yet VERIFIED):

```sh
cmake -S . -B build/phase1 -DCMAKE_BUILD_TYPE=Release
cmake --build build/phase1
ctest --test-dir build/phase1 --output-on-failure
./build/phase1/rc_demo --output build/my-rc-run --ngspice /usr/bin/ngspice
```

With no explicit test path and no `ngspice` on PATH, integration exits 77 and CTest
marks it **Skipped**, with a reason. A configured but missing/broken executable
fails the test. CTest's aggregate "100%" text may include skips; inspect individual
test statuses. A skip cannot satisfy Phase 1 acceptance.

## Backend provisioning used for this run

No ngspice was initially on PATH. Official SourceForge download attempts returned
HTML instead of archives. The MSYS2 mirror denied access; its primary package
server worked. The following packages were downloaded and extracted only under
ignored `build/tools`, without modifying the system installation:

| Package | SHA256 checked against the MSYS2 package record |
| --- | --- |
| `mingw-w64-ucrt-x86_64-ngspice-47-1-any.pkg.tar.zst` | `a9f615da61165c79a904232641e5c3e31fcc5b21c01b1018965b95d543faadf4` |
| `mingw-w64-ucrt-x86_64-fftw-3.3.11-2-any.pkg.tar.zst` | `8bcffc286600ae011726e9356f19ea8afba0a622427c77e89b5a09ecce252d95` |

Sources: [ngspice package](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-ngspice),
[FFTW package](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-fftw).
GCC/OpenMP/readline/termcap runtime DLLs were already available from MSYS2 on PATH.
The following recreates this local extraction layout on a similarly provisioned
host; check the hashes above before extracting or executing downloads:

```powershell
New-Item -ItemType Directory -Force build/tools/msys2-ngspice | Out-Null
Invoke-WebRequest 'https://repo.msys2.org/mingw/ucrt64/mingw-w64-ucrt-x86_64-ngspice-47-1-any.pkg.tar.zst' -OutFile build/tools/ngspice-msys2.pkg.tar.zst
Invoke-WebRequest 'https://repo.msys2.org/mingw/ucrt64/mingw-w64-ucrt-x86_64-fftw-3.3.11-2-any.pkg.tar.zst' -OutFile build/tools/fftw.pkg.tar.zst
Get-FileHash build/tools/ngspice-msys2.pkg.tar.zst,build/tools/fftw.pkg.tar.zst -Algorithm SHA256
tar -xf build/tools/ngspice-msys2.pkg.tar.zst -C build/tools/msys2-ngspice
tar -xf build/tools/fftw.pkg.tar.zst -C build/tools/msys2-ngspice ucrt64/bin/libfftw3-3.dll
```

The extracted package's `spinit` contained absolute `/ucrt64/lib/ngspice/` paths
that did not resolve in the relocated Windows runtime. The first real simulation
was rejected for those startup errors even though it produced samples. Replace
`/ucrt64/` in the extracted `build/tools/msys2-ngspice/ucrt64/share/ngspice/scripts/spinit`
with the absolute extracted prefix ending in `/ucrt64/` (forward slashes). On this
checkout that prefix is `D:/projects/datasim/build/tools/msys2-ngspice/ucrt64/`.
This local runtime correction restored successful code-model loading; the adapter
continues to reject warnings/errors. No third-party binary or runtime script is
included in source control. Normal system installations may not need relocation.

## Outputs and failure behavior

Each successful run keeps `circuit.cir`, raw `samples.dat`, `ngspice.log`,
`version.txt`, `results.csv`, `errors.csv`, and `metadata.txt`. Metadata records the
exact backend banner/path, manual component origin, source/transient/solver
parameters, output filename, metrics and simulation/validation status separately.
Results use `time_s,vin_V,vout_V`; errors use
`time_s,reference_V,error_V,absolute_error_V` over the validation interval.

Invalid physical values fail before starting a run. Missing executables, process
failure/nonzero exit, backend diagnostics, absent/empty/malformed output,
non-finite values and non-increasing times all fail visibly. Incomplete coverage,
wrong source waveform, nonzero pre-step behavior and excessive error also fail.
The CLI returns nonzero and records statuses/error text when a metadata file can
be opened. A failed validation can leave CSV artifacts for diagnosis; consumers
must check `validation_status`, not just file existence. No existing run artifacts
are reused or silently overwritten.

Supported configurations have zero initial voltage, positive finite R/C/amplitude,
positive pulse times, a rise <= 0.001 RC, maximum step <= RC/10, charging-only
duration ending before the first falling edge, and <= one million nominal steps.
This bounds the ideal-step comparison; it is not a general pulse-response solver.

See [validation](PHASE_1_VALIDATION.md) for actual results and limitations.
