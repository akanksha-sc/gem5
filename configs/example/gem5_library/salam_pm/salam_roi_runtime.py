"""ROI hypercall handlers (1999/2000) for SALAM power/thermal sampling."""

from __future__ import annotations

from power_thermal_runtime import (
    remove_trace_files,
    sample_power_now,
    start_power_sampling,
    stop_power_sampling,
)

import m5

from gem5.simulate.exit_handler import ExitHandler

_runtime = {
    "thermal_model": None,
    "bindings": [],
    "trace_debug": False,
    "finalized": False,
}


def configure_salam_roi_runtime(thermal_model, bindings, trace_debug=False):
    _runtime["thermal_model"] = thermal_model
    _runtime["bindings"] = list(bindings or [])
    _runtime["trace_debug"] = trace_debug
    _runtime["finalized"] = False


def _trace(msg: str):
    if _runtime["trace_debug"]:
        print(msg)


def _power_models():
    return [pm for _label, _src, pm in _runtime["bindings"]]


def _start_sampling():
    pms = _power_models()
    if not pms:
        return
    start_power_sampling(pms)
    thermal_model = _runtime["thermal_model"]
    if thermal_model is not None:
        thermal_model.getCCObject().startStepping()


def _stop_sampling(dump_stats=False):
    if _runtime["finalized"]:
        return
    _runtime["finalized"] = True

    pms = _power_models()
    if pms:
        sample_power_now(pms)
        stop_power_sampling(pms)

    thermal_model = _runtime["thermal_model"]
    if thermal_model is not None:
        thermal_model.getCCObject().flushStepNow()
        thermal_model.getCCObject().stopStepping()

    if dump_stats:
        m5.stats.dump()


class SalamBeginRoiHandler(ExitHandler, hypercall_num=1999):
    def _process(self, simulator):
        _trace(
            f"SalamBeginRoiHandler at tick={simulator.get_current_tick()}: "
            "reset stats and start SALAM ROI sampling"
        )
        remove_trace_files(
            m5.options.outdir, "power_trace.csv", "thermal_trace.csv"
        )
        m5.stats.reset()
        _runtime["finalized"] = False
        _start_sampling()

    def _exit_simulation(self):
        return False


class SalamEndRoiHandler(ExitHandler, hypercall_num=2000):
    def _process(self, simulator):
        _trace(
            f"SalamEndRoiHandler at tick={simulator.get_current_tick()}: "
            "stop SALAM ROI sampling and dump stats"
        )
        _stop_sampling(dump_stats=True)

    def _exit_simulation(self):
        return True


def finalize_salam_roi_runtime_if_needed():
    if not _runtime["finalized"]:
        _stop_sampling(dump_stats=False)
