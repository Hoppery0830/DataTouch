"""Run native tests with a host C compiler (GCC/MinGW), not arm-none-eabi-gcc.
Usage: python tests/run_tests.py --cc path/to/gcc.exe
"""
import argparse, os, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--cc',default='gcc');p.add_argument('--selftest-disabled',action='store_true');p.add_argument('--startup-mode',choices=['ACTION_PRESS','ACTION_RUB','ACTION_SLIDE'],default='ACTION_PRESS');p.add_argument('--startup-disabled',action='store_true');args=p.parse_args()
r=Path(__file__).resolve().parents[1];out=r/'build'/'host-tests';out.mkdir(parents=True,exist_ok=True)
inc=['source/module/remote','tests/stubs','source/config','source/bsp/uart','source/module/motor','source/module/buzzer','source/bsp/buzzer','source/task/Motor_Task']
src=['source/module/remote/remote_protocol.c','source/module/remote/remote_control.c','tests/test_framework.c','source/bsp/uart/uart_bsp.c','source/module/motor/emm_protocol.c','source/module/motor/motor.c','source/task/Motor_Task/motor_config.c','source/task/Motor_Task/motor_action.c','source/task/Motor_Task/motor_action_handle.c','source/task/Motor_Task/axis_control_drv.c','source/task/Motor_Task/motor_selftest.c','source/task/Motor_Task/motor_startup.c','source/task/Motor_Task/motor_state_machine.c','source/task/Motor_Task/motor_handle.c','source/module/buzzer/buzzer.c']
env=os.environ.copy()
if Path(args.cc).is_absolute():env['PATH']=str(Path(args.cc).parent)+os.pathsep+env['PATH']
exe=out/('framework_tests.exe' if os.name=='nt' else 'framework_tests')
subprocess.run([args.cc,f'-DAPP_AUTOSTART_MODE={args.startup_mode}',*(['-DAPP_MOTOR_AUTOSTART=0'] if args.startup_disabled else []),*(['-DAPP_MOTOR_SELFTEST=0'] if args.selftest_disabled else []),'-no-canonical-prefixes','-std=c11','-Wall','-Wextra','-Werror','-O0',*[f'-I{r/d}' for d in inc],*[str(r/s) for s in src],'-o',str(exe),'-lm'],check=True,env=env)
subprocess.run([str(exe)],check=True,env=env)
