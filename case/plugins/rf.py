# =============================================================================
#  rf.py — sample REL Python plugin (loaded via test_env.json "python_plugins")
#
#  Registers:
#    vtodbm(v, z) -> power in dBm from a voltage v across impedance z.
#                    `z` has a STATIC default of 50 (ohm), so `vtodbm(1.0)`
#                    uses 50 ohm.
#    vswr(rho)    -> VSWR from reflection-coefficient magnitude |rho|, with a
#                    STATIC default rho = 0.5 (call: `vswr()` or `vswr(0.2)`).
#
#  Both functions are fully vectorized (numpy broadcasting): scalars, vectors
#  and matrices all work, and a scalar default broadcasts against an array
#  argument (e.g. `vtodbm(array, 75.0)`).
#
#  NOTE: names are chosen to avoid colliding with builtin functions (dbm,
#  db, dbmtow, wtodbm already exist in the math library).  The runtime
#  rejects a Python registration whose name is already taken.
# =============================================================================

import numpy as np
import rel


def vtodbm(args):
    v = np.asarray(args["v"])
    z = np.asarray(args["z"])
    power = np.abs(v) ** 2 / z                     # watts; broadcasting
    return 10.0 * np.log10(power / 1e-3)           # dBm


def vswr(args):
    rho = np.abs(np.asarray(args["rho"]))
    if np.any(rho >= 1.0):
        raise ValueError("vswr() requires |rho| < 1, got " + str(rho))
    return (1.0 + rho) / (1.0 - rho)               # broadcasting


rel.register_function("dbm", [
    rel.Param("v"),
    rel.Param("z", 50.0),                          # static default
], vtodbm)

rel.register_function("vswr", [
    rel.Param("rho", 0.5),                         # static default
], vswr)
