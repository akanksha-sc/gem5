# don't use `import *` and import the specific things you need
from m5.objects import (
    BaseMinorCPU,
    BaseO3CPU,
    BaseSimpleCPU,
    Root,
)

# Note that the below dictionaries are a bit hacky
# and are subject to change...

"""
bp_stats = {
    "BTBLookups": "branchPred.BTBLookups",
    "BTBHits": "branchPred.BTBHits",
    "BTBUpdates": "branchPred.BTBUpdates",
    "condPredicted": "branchPred.condPredicted",
    "condIncorrect": "branchPred.condIncorrect",
}

minor_stats = {
    "numCallsReturns": "commitStats0.numCallsReturns",
    "numBranches": "fetchStats0.numBranches",
    "numIntAluAccesses": "executeStats0.numIntAluAccesses",
    "numFpAluAccesses": "executeStats0.numFpAluAccesses",
    "numVecAluAccesses": "executeStats0.numVecAluAccesses",
    "numIntRegReads": "executeStats0.numIntRegReads",
    "numIntRegWrites": "executeStats0.numIntRegWrites",
    "numFpRegReads": "executeStats0.numFpRegReads",
    "numFpRegWrites": "executeStats0.numFpRegWrites",
    "numMiscRegReads": "executeStats0.numMiscRegReads",
    "numMiscRegWrites": "executeStats0.numMiscRegWrites",
    "numCycles": "numCycles",
    "BTBLookups": bp_stats["BTBLookups"],
    "BTBHits": bp_stats["BTBHits"],
    "BTBUpdates": bp_stats["BTBUpdates"],
    "condPredicted": bp_stats["condPredicted"],
    "condIncorrect": bp_stats["condIncorrect"],
}

o3_stats = {
    "numBranches": "fetchStats0.numBranches",
    "numIntAluAccesses": "intAluAccesses",
    "numFpAluAccesses": "fpAluAccesses",
    "numVecAluAccesses": "vecAluAccesses",
    "numIntRegReads": "executeStats0.numIntRegReads",
    "numIntRegWrites": "executeStats0.numIntRegWrites",
    "numFpRegReads": "executeStats0.numFpRegReads",
    "numFpRegWrites": "executeStats0.numFpRegWrites",
    "numMiscRegReads": "executeStats0.numFpRegReads",
    "numMiscRegWrites": "executeStats0.numFpRegWrites",
    "BTBLookups": bp_stats["BTBLookups"],
    "BTBHits": bp_stats["BTBHits"],
    "BTBUpdates": bp_stats["BTBUpdates"],
    "condPredicted": bp_stats["condPredicted"],
    "condIncorrect": bp_stats["condIncorrect"],
}
"""


# Class names should be in CamelCase. Also, this is an abstract base class.
# I.e., no one should ever create an instance of this class.
class AbstractPowerModel:
    def __init__(self, simobj, interval=0, interval_ticks=0):
        # You shouldn't use a list of functions. Instead, implement the
        # dynamic/static power functions in the sub classes
        # using a leading underscore is a good idea so that it's not
        # considered a simobject child
        self._simobj = simobj
        self.name = "AbstractPowerModel"  # should be overridden for debugging
        # I don't like the above, but it's a little hack to make the debugging
        # easier.
        self._interval = interval
        self._interval_ticks = interval_ticks
        self._stats = {}
        self._sample_stats_prepared = False
        self._sample_mode = False

    def sampling_enabled(self) -> bool:
        return self._interval > 0 or self._interval_ticks > 0

    def set_sample_mode(self, enable: bool):
        self._sample_mode = enable
        if enable:
            self._sample_stats_prepared = False

    def clear_sample_state(self):
        """
        Clear Python-side delta bookkeeping.

        This must be called at ROI start after m5.stats.reset(), otherwise the
        next sampled delta can be computed against a pre-reset stat baseline.
        """
        self._stats.clear()
        self._sample_stats_prepared = False
        self._sample_mode = False

    def get_stat(self, stat):
        """Get a stat value.

        In normal mode, return the whole-window total.
        In interval sample mode, return the delta since the previous sample.
        """
        try:
            stat_info = self._simobj.resolveStat(stat)

            if not self.sampling_enabled() or not self._sample_mode:
                return stat_info.total

            self._prepare_stats_for_sample()
            stat_info.prepare()
            total = stat_info.total

            if stat not in self._stats:
                self._stats[stat] = {
                    "value": total,
                    "active": 1,
                    "delta": total,
                }
            elif self._stats[stat]["active"] == 0:
                prev = self._stats[stat]["value"]
                self._stats[stat]["value"] = total
                self._stats[stat]["active"] = 1
                self._stats[stat]["delta"] = total - prev

            return self._stats[stat]["delta"]
        except KeyError:
            print(f"{stat} not found in stats!")
            return 0.0

    def reset_stats_dict(self):
        for stat in self._stats.values():
            stat["active"] = 0
        self._sample_stats_prepared = False

    def _prepare_stats_for_sample(self):
        if not self.sampling_enabled() or self._sample_stats_prepared:
            return
        sim_root = Root.getInstance()
        if sim_root:
            sim_root.preDumpStats()
        self._sample_stats_prepared = True

    def dynamic_power(self) -> float:
        """Returns dynamic power in Watts"""
        # These should not be implemented in this (abstract) base class
        raise NotImplementedError

    def static_power(self) -> float:
        """Returns static power in Watts"""
        # These should not be implemented in this (abstract) base class
        raise NotImplementedError

    def check_cpu_type(self, core):
        if isinstance(core, BaseMinorCPU):
            return 0
        elif isinstance(core, BaseSimpleCPU):
            return 1
        elif isinstance(core, BaseO3CPU):
            return 2

    def getExecutionTime(self):
        """
        Return the denominator used to convert event energy to Watts.

        Normal stats path:
            Use full simulated/stat window time.

        Interval-sampling path:
            Use the actual active sample duration, but only while the
            PowerModelPyFunc C++ wrapper is actively evaluating an interval
            sample.
        """
        if getattr(self, "_sample_mode", False):
            try:
                dur = self.getSampleDurationSeconds()
                if dur > 0:
                    return dur
            except Exception:
                pass

            if self._interval_ticks > 0:
                return self._interval_ticks / 1e12

            clk_domain = self._simobj.clk_domain.clock.getValue()[0]
            return self._interval / (1e12 / clk_domain)

        return Root.getInstance().resolveStat("simSeconds").total

    def convert_to_watts(self, value: float) -> float:
        """Convert energy in nanojoules to Watts"""
        time = self.getExecutionTime()
        if time == 0:
            return 0.0
        value_in_j = value * 1e-9
        return value_in_j / time
