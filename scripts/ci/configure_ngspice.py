"""Normalize only MSYS-rooted spinit path arguments; verify the real runtime.

CI provisioning helper, not part of the Datasim application or numerical model.
"""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


PATH_COMMAND = re.compile(
    r'^(?P<head>[ \t]*(?:codemodel|osdi)[ \t]+)'
    r'(?P<token>"[^"\r\n]*"|[^\s"]+)(?P<tail>[^\r\n]*)(?P<newline>\r?\n)?$',
    re.IGNORECASE,
)


def normalize_spinit(text, prefix):
    """Preserve all other text, including native path tokens and comments."""
    prefix = prefix.replace('\\', '/').rstrip('/')
    if not re.fullmatch(r'[A-Za-z]:/[^"\r\n]*', prefix):
        raise ValueError('prefix must be a native Windows absolute directory')
    output, paths = [], []
    for line in text.splitlines(keepends=True):
        match = PATH_COMMAND.fullmatch(line)
        if not match:
            output.append(line)
            continue
        token = match['token']
        quoted = token.startswith('"')
        path = token[1:-1] if quoted else token
        converted = path.startswith('/ucrt64/')
        if converted:
            path = prefix + path[len('/ucrt64'):]
            rendered = '"' + path + '"' if quoted or any(c.isspace() for c in path) else path
            line = match['head'] + rendered + match['tail'] + (match['newline'] or '')
        # Never try to guess how to repair a pre-corrupted or unknown path.
        if not re.match(r'^[A-Za-z]:[/\\]', path) or len(re.findall(r'[A-Za-z]:', path)) != 1:
            raise ValueError('unsupported or corrupted spinit path: ' + path)
        paths.append({'command': match['head'].strip().lower(), 'path': path, 'converted': converted})
        output.append(line)
    return ''.join(output), paths


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--spinit', type=Path, required=True)
    parser.add_argument('--prefix', required=True)
    parser.add_argument('--ngspice', type=Path, required=True)
    parser.add_argument('--diagnostics', type=Path, required=True)
    args = parser.parse_args()
    diagnostics = args.diagnostics.resolve()
    diagnostics.mkdir(parents=True, exist_ok=False)
    before = args.spinit.read_bytes()
    (diagnostics / 'spinit.before').write_bytes(before)
    after_text, paths = normalize_spinit(before.decode('utf-8'), args.prefix)
    again, second_paths = normalize_spinit(after_text, args.prefix)
    if again != after_text or any(item['converted'] for item in second_paths):
        raise RuntimeError('spinit conversion is not idempotent')
    models = [item for item in paths if item['command'] == 'codemodel']
    if not models:
        raise RuntimeError('no code-model directives found')
    for item in models:
        if not Path(item['path']).is_file():
            raise RuntimeError('code-model file missing: ' + item['path'])
    after = after_text.encode('utf-8')
    if after != before:
        args.spinit.write_bytes(after)
    (diagnostics / 'spinit.after').write_bytes(args.spinit.read_bytes())
    print(json.dumps(paths, indent=2), flush=True)
    # Load the installed spinit in a real process. -n suppresses user spiceinit,
    # not package spinit. Errors/warnings remain fatal, just as in the C++ backend.
    (diagnostics / 'runtime-check.cir').write_text(
        'Datasim ngspice runtime initialization check\n.control\n'
        'echo DATASIM_RUNTIME_READY\nquit\n.endc\n.end\n', encoding='utf-8')
    with (diagnostics / 'runtime-check.log').open('wb') as log:
        process = subprocess.run(
            [str(args.ngspice.resolve()), '-n', '-b', 'runtime-check.cir'],
            cwd=diagnostics, stdout=log, stderr=subprocess.STDOUT,
            stdin=subprocess.DEVNULL, timeout=30, check=False)
    log = (diagnostics / 'runtime-check.log').read_text(encoding='utf-8', errors='replace')
    print(log, flush=True)
    if process.returncode or 'DATASIM_RUNTIME_READY' not in log or any(
            word in log.lower() for word in ('error', 'fatal', 'warning', 'aborted')):
        raise RuntimeError('ngspice initialization failed; inspect runtime-check.log')
    report = {'status': 'PASSED', 'code_models_checked': len(models),
              'converted_paths': sum(item['converted'] for item in paths),
              'idempotent': True, 'backend_exit_code': process.returncode,
              'before_sha256': hashlib.sha256(before).hexdigest(),
              'after_sha256': hashlib.sha256(after).hexdigest(), 'paths': paths}
    (diagnostics / 'runtime-report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
