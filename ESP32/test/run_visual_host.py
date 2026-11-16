"""Compile and run the real outbox against a simulated NVS and task queues."""
import argparse, os, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument("--cxx",default="g++");a=p.parse_args()
r=Path(__file__).resolve().parents[1]
out=r/".pio"/"visual_host.exe"
out.parent.mkdir(exist_ok=True)
env=os.environ.copy();env["PATH"]=str(Path(a.cxx).parent)+os.pathsep+env["PATH"]
subprocess.run([a.cxx,"-no-canonical-prefixes","-std=c++17","-Wall","-Wextra","-Werror",
 "-I"+str(r/"test/visual_host"),"-I"+str(r/"src"),
 str(r/"test/visual_host/test.cpp"),str(r/"src/visual_result.cpp"),
 str(r/"src/remote_protocol.cpp"),"-o",str(out)],check=True,env=env)
subprocess.run([str(out)],check=True,env=env)
