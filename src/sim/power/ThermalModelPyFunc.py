from m5.params import *
from m5.SimObject import SimObject
from m5.util.pybind import *


class ThermalModelPyFunc(SimObject):
    type = "ThermalModelPyFunc"
    cxx_header = "sim/power/thermal_model_pyfunc.hh"
    cxx_class = "gem5::ThermalModelPyFunc"

    solve = Param.PyFunc("Solver callback invoked once per thermal step")
    reset = Param.PyFunc(
        lambda initial_temp_k: None,
        "Optional solver reset callback invoked when stepping starts",
    )
