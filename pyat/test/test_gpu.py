import at
import machine_data
import sys
if sys.version_info.minor < 9:
    from importlib_resources import files, as_file
else:
    from importlib.resources import files, as_file

def hmba_lattice():
    with as_file(files(machine_data) / 'hmba.mat') as path:
        ring = at.load_lattice(path)
    return ring
import numpy as np


# Perfrom a test
try:
    ring = hmba_lattice()
    rin = np.array([0.001,0.0,0.001,0.0,0.0,0.0])
    rout, *_ = ring.track(rin,nturns=1,use_gpu=True,gpu_pool=[4])
    print(rout)
except Exception as e:
    print(e)
