#!/usr/bin/env python3
"""Create a fresh public SOURCE candidate; never publish or copy .git/history.

All copied files are allowlisted; all text is scanned before any file is written.
Use a new destination each time. This does not modify the device checkout.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys

from public_audit import ROOT, scan

FIXED = [
    'README.md', 'LICENSE', '.gitignore', 'SECURITY.md', 'CONTRIBUTING.md',
    'NOTICE', 'docs/openclaw-integration.md',
    'hardware/PARTS.md', 'assets/README.md',
    'assets/claw-buoy-hero-v4.jpg', 'assets/claw-buoy-comic-v1.png',
    '.github/workflows/public-ci.yml',
    'tools/public_audit.py', 'tools/public_export.py', 'tools/public_checks.py',
    'firmware/esp-openclaw-node/LICENSE',
    'firmware/esp-openclaw-node/SNAPSHOT.md',
]
BASE = 'firmware/esp-openclaw-node/'
APP = BASE + 'examples/esp32-node/'
PATTERNS = [
    BASE + 'components/*/CMakeLists.txt',
    BASE + 'components/*/Kconfig',
    BASE + 'components/*/idf_component.yml',
    BASE + 'components/*/LICENSE',
    BASE + 'components/*/*.c',
    BASE + 'components/*/include/*.h',
    BASE + 'components/*/src/*.c',
    BASE + 'components/*/private_include/*.h',
    BASE + 'components/*/private_include/vendor/*.h',
    BASE + 'components/*/private_include/vendor/*.LICENSE',
    APP + 'CMakeLists.txt', APP + 'partitions.csv',
    APP + 'sdkconfig.defaults', APP + 'sdkconfig.claw-box.defaults',
    APP + 'dependencies.lock',
    APP + 'main/CMakeLists.txt', APP + 'main/Kconfig.projbuild',
    APP + 'main/idf_component.yml', APP + 'main/*.c', APP + 'main/*.h',
    APP + 'tests/*.c', APP + 'tests/stubs/*.h',
    APP + 'tests/stubs/driver/*.h', APP + 'tests/stubs/freertos/*.h',
    'design/renders/claw-buoy-dot-concept-idle-v1.png',
    'design/renders/claw-buoy-message-v1.png',
    'hardware/enclosure/v5/*.stl', 'hardware/enclosure/v5/*.scad',
    'hardware/enclosure/v5/*.py', 'hardware/enclosure/v5/preview.png',
    'hardware/enclosure/v5/AUDIT.md', 'hardware/enclosure/v5/README.txt',
]
# Generated guides deliberately replace private deployment notes, not source.
GENERATED = {
    'firmware/README.md': '# Firmware\n\nBuild and wiring: see the [root README](../README.md).\n\nThe source snapshot preserves upstream licensing. Local device release archives, credentials, build output, and test evidence are deliberately not distributed.\n',
    APP + 'CLAW_IN_A_BOX.md': '# Board-specific build and wiring\n\nSee the [root README](../../../../README.md) for current build commands and GPIO assignments. Touch is enabled; backlight control is GPIO17. GPIO16 is reserved for the onboard RGB LED on the documented board.\n',
    APP + '.gitignore': 'build/\nbuild-*/\nmanaged_components/\nsdkconfig\nsdkconfig.claw-box\nsdkconfig.old\nsdkconfig.claw-box.old\n',
}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    dest = args.destination.expanduser().resolve()
    if dest.exists() or dest == ROOT or ROOT in dest.parents:
        parser.error('destination must not exist and must be outside this source checkout')
    names = set(FIXED)
    for pattern in PATTERNS:
        names.update(str(p.relative_to(ROOT)) for p in ROOT.glob(pattern) if p.is_file())
    files = {}
    for name in sorted(names):
        path = ROOT / name
        if path.is_symlink() or not path.is_file():
            parser.error('missing or symlinked allowlisted file: ' + name)
        files[name] = path.read_bytes()
    files.update({name: value.encode() for name, value in GENERATED.items()})
    findings = []
    for name, data in files.items():
        findings.extend(dict(path=name, line=line, rule=rule) for line, rule in scan(data))
    if findings:
        print(json.dumps({'status': 'blocked', 'redacted_findings': findings}, indent=2))
        return 1
    dest.mkdir(parents=True)
    for name, data in files.items():
        path = dest / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    manifest = {name: hashlib.sha256(data).hexdigest() for name, data in sorted(files.items())}
    (dest / 'PUBLIC_MANIFEST.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps({'status': 'candidate-only', 'files': len(files), 'destination': str(dest),
                      'history_included': False, 'heuristic_findings': 0}, indent=2))
    return 0

if __name__ == '__main__':
    sys.exit(main())
