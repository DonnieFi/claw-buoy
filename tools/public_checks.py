#!/usr/bin/env python3
"""Run host regression tests without SDK, gateway access, or device state."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / 'firmware/esp-openclaw-node/examples/esp32-node'
CC = os.environ.get('CC', 'cc')
with tempfile.TemporaryDirectory(prefix='claw-buoy-checks-') as tmp:
    for name, sources, includes in [
        ('ui', ['tests/ui_regression.c'], ['main']),
        ('touch', ['tests/touch_regression.c', 'main/claw_box_touch.c'], ['tests/stubs', 'main']),
    ]:
        binary = str(Path(tmp) / name)
        command = [CC, '-std=c11', '-Wall', '-Wextra']
        command += ['-I' + str(APP / inc) for inc in includes]
        command += [str(APP / src) for src in sources]
        subprocess.run(command + ['-lm', '-o', binary], check=True)
        subprocess.run([binary], check=True)
