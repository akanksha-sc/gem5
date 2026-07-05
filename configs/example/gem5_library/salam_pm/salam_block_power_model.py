from m5.objects import PowerModel

from .salam_block_power import (
    SalamBlockPowerOff,
    SalamBlockPowerOn,
)


class SalamBlockPowerModel(PowerModel):
    def __init__(
        self,
        stat_source,
        block,
        label,
        interval=0,
        interval_ticks=0,
        trace_debug=False,
    ):
        super().__init__()
        self.pm = [
            SalamBlockPowerOn(
                stat_source,
                block,
                label,
                interval,
                interval_ticks,
                trace_debug,
            ),
            SalamBlockPowerOff(label),
            SalamBlockPowerOff(label),
            SalamBlockPowerOff(label),
        ]
