#!/usr/bin/env python3
"""Exercise source identity stability and sensitivity in a disposable build."""
from __future__ import annotations

import argparse
import re
import subprocess
from pathlib import Path


def configure(cmake: str, root: Path, build: Path) -> str:
    subprocess.run([cmake, "-S", str(root), "-B", str(build)], check=True,
                   stdout=subprocess.DEVNULL)
    header = (build / "generated/datasim/build_info.hpp").read_text(encoding="utf-8")
    match = re.search(r'DATASIM_SOURCE_SHA256 "([0-9a-f]{64})"', header)
    if not match:
        raise RuntimeError("DATASIM_SOURCE_SHA256 missing from generated build_info.hpp")
    return match.group(1)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--cmake", default="cmake")
    args = parser.parse_args()
    root, build = args.source.resolve(), args.build.resolve()
    header = root / "apps/datasim_viewer/waveform_widget.hpp"
    fixture = root / "tests/fixtures/synthetic_datasheet.pdf"
    header_bytes, fixture_bytes = header.read_bytes(), fixture.read_bytes()
    try:
        baseline = configure(args.cmake, root, build)
        unchanged = configure(args.cmake, root, build)
        header.write_bytes(header_bytes + b"\n")
        changed_header = configure(args.cmake, root, build)
        header.write_bytes(header_bytes)
        fixture.write_bytes(fixture_bytes + b"\0")
        changed_fixture = configure(args.cmake, root, build)
        fixture.write_bytes(fixture_bytes)
        (build / "identity-excluded-probe.tmp").write_bytes(b"outside source scope")
        excluded = configure(args.cmake, root, build)
        (build / "identity-excluded-probe.tmp").unlink()
    finally:
        header.write_bytes(header_bytes)
        fixture.write_bytes(fixture_bytes)
    restored = configure(args.cmake, root, build)
    if not (baseline == unchanged == excluded == restored):
        raise RuntimeError("unchanged, excluded, or restored source identity differed")
    if baseline in (changed_header, changed_fixture) or changed_header == changed_fixture:
        raise RuntimeError("application header and fixture mutations were not distinguished")
    print(f"VERIFIED DATASIM_SOURCE_SHA256 baseline={baseline}")
    print("unchanged stable; application header and synthetic fixture each changed identity;")
    print("build-directory probe excluded; exact source restore returned to baseline")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
