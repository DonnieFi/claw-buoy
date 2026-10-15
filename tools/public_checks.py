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

    # Exercise configurable daytime, overnight and disabled schedules independently.
    probe = Path(tmp) / 'schedule.c'
    probe.write_text('#include "buoy_ui_rules.h"\nint main(void) { for(int h=0;h<24;h++) { int want = EXPECT_MODE==0 ? 0 : EXPECT_MODE==1 ? (h>=9 && h<17) : (h>=22 || h<7); if (!!buoy_dark_hour(h)!=!!want) return 1; } return 0; }\n')
    for start, end, mode in [(9,17,1),(22,7,2),(7,7,0)]:
        binary = str(Path(tmp) / 'schedule')
        subprocess.run([CC, '-std=c11', '-I'+str(APP/'main'),
                        f'-DBUOY_DARK_START_HOUR={start}', f'-DBUOY_DARK_END_HOUR={end}',
                        f'-DEXPECT_MODE={mode}', str(probe), '-o', binary], check=True)
        subprocess.run([binary], check=True)
    print('PASS: configurable daytime, overnight and disabled dark schedules')
