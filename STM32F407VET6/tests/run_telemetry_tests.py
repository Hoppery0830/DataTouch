import argparse, os, subprocess, shutil
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--cc',default='gcc');a=p.parse_args()
r=Path(__file__).resolve().parents[1];out=r/'build/host-tests';out.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();cc=Path(shutil.which(a.cc) or a.cc).absolute();env['PATH']=str(cc.parent)+os.pathsep+env['PATH']
exe=out/'telemetry_tests.exe'
src=['tests/test_telemetry.c','source/module/touch/touch_telemetry.c','source/module/touch/k230_link.c','source/module/remote/remote_protocol.c']
inc=['source/config','source/module/touch','source/module/remote']
subprocess.run([str(cc),'-no-canonical-prefixes','-std=c11','-Wall','-Wextra','-Werror','-O2',*[str(r/s) for s in src],*[f'-I{r/i}' for i in inc],'-o',str(exe),'-lm'],env=env,check=True)
subprocess.run([str(exe)],env=env,check=True)
