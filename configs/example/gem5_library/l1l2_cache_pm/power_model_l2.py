from m5.objects import (
    L2XBar,
    Port,
    PowerModel,
    PowerModelPyFunc,
    SystemXBar,
)
from m5.stats import *

from .power_model_l2on import L2PowerOn


class L2PowerOff(PowerModelPyFunc):
    def __init__(self):
        super().__init__()
        self.dyn = lambda: 0.0
        self.st = lambda: 0.0


class L2PowerModel(PowerModel):
    def __init__(
        self,
        l2cache,
        writeback,
        interval,
        interval_ticks=0,
        trace_debug=False,
        static_regression=None,
        **kwargs,
    ):
        super().__init__(**kwargs)
        # Choose a power model for every power state
        self.pm = [
            L2PowerOn(
                l2cache,
                writeback,
                interval,
                interval_ticks,
                trace_debug,
                static_regression,
            ),  # ON
            L2PowerOff(),  # CLK_GATED
            L2PowerOff(),  # SRAM_RETENTION
            L2PowerOff(),  # OFF
        ]
