from m5.objects import BaseO3CPU

from .base_power_model import AbstractPowerModel
from .mcpat_power_model import McPATPowerModel


class O3McPATRfPower(McPATPowerModel):
    # avoid the use of default values
    def __init__(
        self, cpu: BaseO3CPU, act_energies, interval=0, interval_ticks=0
    ):
        super().__init__(cpu, act_energies, interval, interval_ticks)
        # _rf isn't really needed since you have `_simobj`
        self.name = "InorderMcPatRFPowerModel"

    def print_mcpat(self, indent):
        int_rf_energy = self.int_energy()
        fp_rf_energy = self.fp_energy()
        print(" " * indent + f"Register Files:")
        print(
            " " * (indent + 2)
            + f"Runtime Dynamic = {self.convert_to_watts(int_rf_energy + fp_rf_energy)} W\n"
        )
        print(" " * (indent + 4) + f"Integer RF:")
        print(
            " " * (indent + 6)
            + f"Runtime Dynamic = {self.convert_to_watts(int_rf_energy)} W\n"
        )
        print(" " * (indent + 4) + f"Floating Point RF:")
        print(
            " " * (indent + 6)
            + f"Runtime Dynamic = {self.convert_to_watts(fp_rf_energy)} W\n"
        )

    def static_power(self) -> float:
        return 1.0

    def dynamic_power(self) -> float:
        energy = self.int_energy() + self.fp_energy()
        return self.convert_to_watts(energy)

    def int_energy(self) -> float:
        reads = self.get_stat("executeStats0.numIntRegReads").total
        writes = self.get_stat("executeStats0.numIntRegWrites").total
        return (
            reads * self._act_energies["IntRegFile"]["Read"]
            + writes * self._act_energies["IntRegFile"]["Write"]
        )

    def fp_energy(self) -> float:
        reads = self.get_stat("executeStats0.numFpRegReads").total
        writes = self.get_stat("executeStats0.numFpRegWrites").total

        return (
            reads * self._act_energies["FpRegFile"]["Read"]
            + writes * self._act_energies["FpRegFile"]["Write"]
        )
