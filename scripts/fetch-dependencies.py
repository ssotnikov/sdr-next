#!/usr/bin/env python3
"""Acquire pinned build dependencies outside the checkout; never install drivers."""
import argparse
import json
import os
from pathlib import Path
import platform
import subprocess
import sys


PROJECT = Path(__file__).resolve().parent.parent


def run(*args, cwd=None, capture=False, env=None):
    print('+ ' + ' '.join(map(str, args)), flush=True)
    result = subprocess.run(list(map(str, args)), cwd=cwd, env=env, check=True,
                            text=True, stdout=subprocess.PIPE if capture else None)
    return result.stdout.strip() if capture else None


def checkout(path, spec):
    if not path.exists():
        path.mkdir(parents=True)
    if not (path / '.git').exists():
        if any(path.iterdir()):
            raise ValueError(f'Not a dedicated dependency checkout: {path}')
        run('git', 'init', path)
        run('git', 'remote', 'add', 'origin', spec['repository'], cwd=path)
    if Path(run('git', 'rev-parse', '--show-toplevel', cwd=path, capture=True)).resolve() != path:
        raise ValueError(f'Not a dedicated dependency checkout: {path}')
    if run('git', 'remote', 'get-url', 'origin', cwd=path, capture=True) != spec['repository']:
        raise ValueError(f'Unexpected repository at {path}; choose a fresh dependency root')
    if run('git', 'status', '--porcelain', cwd=path, capture=True):
        raise ValueError(f'Modified dependency checkout at {path}; it will not be reset')
    current = subprocess.run(['git', 'rev-parse', '--verify', 'HEAD'], cwd=path,
                             text=True, capture_output=True, check=False)
    if current.returncode == 0 and current.stdout.strip() == spec['commit']:
        print(f'Pinned checkout already available: {path}', flush=True)
        return
    run('git', 'fetch', '--depth=1', 'origin', spec['commit'], cwd=path)
    run('git', 'checkout', '--detach', spec['commit'], cwd=path)
    if run('git', 'rev-parse', 'HEAD', cwd=path, capture=True) != spec['commit']:
        raise ValueError(f'Commit mismatch at {path}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True,
                        help='Dedicated dependency directory outside the project')
    parser.add_argument('--qt-source', action='store_true',
                        help='Also fetch the pinned Qt source subset (does not build an SDK)')
    parser.add_argument('--hardware', action='store_true',
                        help='Build SoapySDR via vcpkg; requires the native C++ toolchain')
    args = parser.parse_args()
    root = args.root.resolve()
    if root == PROJECT or PROJECT in root.parents or root in PROJECT.parents:
        raise ValueError('Use a dedicated dependency directory outside the project')
    lock = json.loads((PROJECT / 'dependencies.lock.json').read_text(encoding='utf-8'))
    manifest = json.loads((PROJECT / 'vcpkg.json').read_text(encoding='utf-8'))
    if lock['schema_version'] != 1 or manifest['builtin-baseline'] != lock['vcpkg']['commit']:
        raise ValueError('Dependency lock and vcpkg manifest baseline disagree')
    if platform.system() not in ('Windows', 'Linux') or platform.machine().lower() not in (
            'amd64', 'x86_64'):
        raise ValueError('Only Windows/Linux x86-64 dependency profiles are defined')
    root.mkdir(parents=True, exist_ok=True)
    vcpkg = root / 'vcpkg'
    checkout(vcpkg, lock['vcpkg'])
    if args.qt_source:
        qt = root / 'qt-source'
        checkout(qt, lock['qt'])
        modules = lock['qt']['modules']
        for name, commit in modules.items():
            entry = run('git', 'ls-tree', 'HEAD', name, cwd=qt, capture=True)
            if entry.split()[:3] != ['160000', 'commit', commit]:
                raise ValueError(f'Qt lock does not match superproject: {name}')
        run('git', 'submodule', 'init', '--', *modules, cwd=qt)
        for name, commit in modules.items():
            checkout(qt / name, {'repository': f'https://github.com/qt/{name}.git',
                                 'commit': commit})
            if run('git', 'rev-parse', 'HEAD', cwd=qt / name, capture=True) != commit:
                raise ValueError(f'Qt module commit mismatch: {name}')
    if args.hardware:
        windows = platform.system() == 'Windows'
        env = dict(os.environ, VCPKG_DISABLE_METRICS='1')
        env.setdefault('VCPKG_MAX_CONCURRENCY', '2')
        if windows:
            vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / (
                'Microsoft Visual Studio/Installer/vswhere.exe')
            installation = run(vswhere, '-latest', '-products', '*', '-version', '[18.0,19.0)',
                               '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
                               '-property', 'installationPath', capture=True)
            if not installation:
                raise ValueError('Visual Studio 2026 C++ tools are required')
            env['VCPKG_VISUAL_STUDIO_PATH'] = installation
            run('cmd.exe', '/c', str(vcpkg / 'bootstrap-vcpkg.bat'), '-disableMetrics', env=env)
        else:
            run('sh', vcpkg / 'bootstrap-vcpkg.sh', '-disableMetrics', env=env)
        triplet = 'x64-windows' if windows else 'x64-linux-dynamic'
        run(vcpkg / ('vcpkg.exe' if windows else 'vcpkg'), 'install',
            f'--triplet={triplet}', f'--x-manifest-root={PROJECT}',
            f'--x-install-root={root / "installed"}', '--x-feature=hardware',
            f'--downloads-root={root / "downloads"}', '--disable-metrics', env=env)
    print('Pinned dependency acquisition complete. Qt sources are not a built SDK.', flush=True)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(f'Dependency acquisition failed: {error}', file=sys.stderr)
        sys.exit(1)
