from m5.objects.PowerModelState import PowerModelState
from m5.params import *
from m5.SimObject import (
    SimObject,
    cxxMethod,
)
from m5.util.pybind import *


# Dynamic and static power equations represented by arithmetic operators
# than strings in MathExprPowerModel
class PowerModelPyFunc(PowerModelState):
    type = "PowerModelPyFunc"
    cxx_header = "sim/power/power_model_pyfunc.hh"
    cxx_class = "gem5::PowerModelPyFunc"

    # Equations for dynamic and static power in Watts
    # Equations may use gem5 stats ie. "1.1*ipc + 2.3*l2_cache.overall_misses"
    # It is possible to use automatic variables such as "temp"
    # You may also use stat names (relative path to the simobject)
    dyn = Param.PyFunc("Function to call for Dynamic Power")
    st = Param.PyFunc("Function to call for Static Power")
    reset = Param.PyFunc(lambda: None, "Reset Python-side sampling state")

    pwr_interval = Param.Cycles(0, "Interval in which power is calculated")
    pwr_interval_ticks = Param.Tick(
        0,
        "Absolute simulated tick interval for power sampling. If nonzero, "
        "this overrides pwr_interval. If zero, legacy pwr_interval cycles "
        "are converted to ticks using attached ClockedObject clock period",
    )
    enable_trace = Param.Bool(
        False,
        "Enable sampled power trace output for debugging",
    )
    trace_file = Param.String("", "Optional CSV path for sampled power trace")
    trace_label = Param.String(
        "",
        "Optional stable label for sampled power traces. Falls back to the "
        "attached ClockedObject name.",
    )
    auto_start = Param.Bool(
        False,
        "Automatically start sampled power collection at SimObject startup. "
        "Default false preserves ROI-controlled behavior.",
    )
    cxx_exports = [
        PyBindMethod("startSampling"),
        PyBindMethod("stopSampling"),
        PyBindMethod("sampleNow"),
        PyBindMethod("isSamplingActive"),
        PyBindMethod("inPowerAtInterval"),
        PyBindMethod("clearCachedSample"),
        PyBindMethod("clearAccumulatedPower"),
        PyBindMethod("getCachedDynamicPower"),
        PyBindMethod("getCachedStaticPower"),
        PyBindMethod("getCachedTotalPower"),
        PyBindMethod("getCachedPowerTick"),
        PyBindMethod("getCachedTemperatureKelvin"),
        PyBindMethod("getAccumulatedDynamicPower"),
        PyBindMethod("getAccumulatedStaticPower"),
        PyBindMethod("getAccumulatedTotalPower"),
        PyBindMethod("getAccumulatedPowerTick"),
        PyBindMethod("getAccumulatedPowerDurationTicks"),
        PyBindMethod("getAccumulatedPowerSampleCount"),
        PyBindMethod("getTemperatureKelvin"),
        PyBindMethod("getSampleDurationSeconds"),
        PyBindMethod("getEffectiveIntervalTicks"),
    ]
