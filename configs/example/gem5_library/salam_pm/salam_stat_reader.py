from mcpat_power_model.inorder_mcpat_power_model.base_power_model import (
    AbstractPowerModel,
)


class SalamStatReader(AbstractPowerModel):
    """Delta stat reader over a SALAMPowerModel SimObject."""

    def __init__(self, stat_source, interval=0, interval_ticks=0):
        super().__init__(stat_source, interval, interval_ticks)
        self.model_name = "SalamStatReader"
