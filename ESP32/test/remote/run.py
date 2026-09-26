from pathlib import Path
import subprocess,os
r=Path(__file__).resolve().parents[2]
cc=Path('D:/Program/DATA_TOUCH/DataTouchCode/build/host-toolchain/bin/g++.exe')
out=r/'.pio'/'host-remote.exe';env=os.environ.copy();env['PATH']=str(cc.parent)+os.pathsep+env['PATH']
subprocess.run([str(cc),'-no-canonical-prefixes','-std=c++11','-Wall','-Wextra','-Werror','-I'+str(r/'test/remote'),'-I'+str(r/'src'),str(r/'test/remote/test_link.cpp'),str(r/'src/remote_link.cpp'),str(r/'src/remote_protocol.cpp'),'-o',str(out)],check=True,env=env)
subprocess.run([str(out)],check=True,env=env)
