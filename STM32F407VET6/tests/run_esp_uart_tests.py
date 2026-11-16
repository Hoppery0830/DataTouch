"""Exercise the real ESP UART BSP with host HAL stubs."""
import argparse
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--cc', default='gcc')
args = p.parse_args()
r = Path(__file__).resolve().parents[1]
out = r / 'build' / 'host-tests'
out.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
if Path(args.cc).is_absolute():
    env['PATH'] = str(Path(args.cc).parent) + os.pathsep + env['PATH']
exe = out / ('esp_uart_tests.exe' if os.name == 'nt' else 'esp_uart_tests')
includes = ['tests/stubs', 'source/bsp/esp_uart', 'source/bsp/touch',
            'source/config', 'source/module/remote']
sources = ['tests/test_esp_uart.c', 'source/bsp/esp_uart/esp_uart.c',
           'source/module/remote/remote_protocol.c']
subprocess.run([args.cc, '-no-canonical-prefixes', '-std=c11', '-Wall', '-Wextra', '-Werror',
                *['-I' + str(r / d) for d in includes],
                *[str(r / s) for s in sources], '-o', str(exe)], check=True, env=env)
subprocess.run([str(exe)], check=True, env=env)
