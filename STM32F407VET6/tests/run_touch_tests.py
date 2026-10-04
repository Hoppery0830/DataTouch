"""Native protocol and numerical tests; --cc must name a host compiler, not ARM GCC."""
import argparse
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--cc', default='gcc')
args = p.parse_args()
root = Path(__file__).resolve().parents[1]
out = root / 'build' / 'host-tests'
out.mkdir(parents=True, exist_ok=True)
exe = out / ('touch_tests.exe' if os.name == 'nt' else 'touch_tests')
env = os.environ.copy()
if Path(args.cc).is_absolute():
    env['PATH'] = str(Path(args.cc).parent) + os.pathsep + env['PATH']
subprocess.run([args.cc, '-no-canonical-prefixes', '-std=c11', '-Wall', '-Wextra', '-Werror', '-O2',
               '-I' + str(root / 'source/config'), '-I' + str(root / 'source/module/touch'),
               str(root / 'tests/test_touch.c'), str(root / 'source/module/touch/touch_processing.c'),
               '-o', str(exe), '-lm'], check=True, env=env)
subprocess.run([str(exe)], check=True, env=env)
