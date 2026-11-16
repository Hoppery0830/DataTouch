"""Hardware-free capture interlock regression tests."""
import argparse, os, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--cc', default='gcc');args=p.parse_args()
r=Path(__file__).resolve().parents[1];out=r/'build'/'host-tests';out.mkdir(parents=True,exist_ok=True)
env=os.environ.copy()
if Path(args.cc).is_absolute():env['PATH']=str(Path(args.cc).parent)+os.pathsep+env['PATH']
exe=out/('visual_tests.exe' if os.name=='nt' else 'visual_tests')
subprocess.run([args.cc,'-no-canonical-prefixes','-std=c11','-Wall','-Wextra','-Werror',
    '-I'+str(r/'source/module/touch'),'-I'+str(r/'source/config'),
    str(r/'tests/test_visual_result.c'),str(r/'source/module/touch/visual_result.c'),'-o',str(exe)],check=True,env=env)
subprocess.run([str(exe)],check=True,env=env)
