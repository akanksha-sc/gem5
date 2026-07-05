from m5.objects import Root

from .base_power_model import AbstractPowerModel


class McPATPowerModel(AbstractPowerModel):
    def __init__(self, simobj, xml_tree=None, interval=0, interval_ticks=0):
        super().__init__(simobj, interval, interval_ticks)
        self.name = "McPATPowerModel"
        self._xml_tree = xml_tree
        self._act_energy_tree_root = (
            self._xml_tree.getroot() if self._xml_tree is not None else None
        )

    def convert_to_watts(self, value: float) -> float:
        """Note that McPAT AEs are already in terms of J,
        no need for conversion"""

        time = self.getExecutionTime()
        if time == 0:
            return 0.0
        return value / time
