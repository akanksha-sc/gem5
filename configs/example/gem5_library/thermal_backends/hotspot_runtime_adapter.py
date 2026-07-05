import math

from .hotspot_solver import HotSpotSolver


class HotSpotRuntimeAdapter:
    """
    Thin runtime adapter that keeps HotSpot entirely behind the generic
    ThermalModelPyFunc backend hook.

    domain_data contract:
        [(label, sampled_power_w, current_temp_k), ...]

    The labels must match the HotSpot floorplan block names exactly.
    """

    def __init__(
        self, floorplan, layers, config, method="rk4", max_external_step_s=1e-6
    ):
        if method not in ("rk4", "euler"):
            raise ValueError(f"Invalid HotSpot method: {method}")

        self._expected_labels = [entry["name"] for entry in floorplan]
        if len(self._expected_labels) != len(set(self._expected_labels)):
            raise ValueError(
                f"Duplicate HotSpot floorplan labels: {self._expected_labels}"
            )
        self._expected_label_set = set(self._expected_labels)
        self._solver = HotSpotSolver(floorplan, layers, config)
        self._method = method
        self._max_external_step_s = float(max_external_step_s)
        if not math.isfinite(self._max_external_step_s) or (
            self._max_external_step_s <= 0
        ):
            raise ValueError(
                f"Invalid max_external_step_s: {self._max_external_step_s}"
            )

    def reset(self, initial_temp_k):
        initial_temp_k = float(initial_temp_k)
        if not math.isfinite(initial_temp_k) or initial_temp_k <= 0:
            raise ValueError(
                f"Invalid HotSpot reset temperature: {initial_temp_k}"
            )
        self._solver.reset(initial_temp_K=initial_temp_k)

    def solve(self, domain_data, dt_seconds):
        dt_seconds = float(dt_seconds)
        if not math.isfinite(dt_seconds) or dt_seconds <= 0:
            raise ValueError(f"Invalid thermal dt_seconds: {dt_seconds}")

        entries = list(domain_data)
        seen = [name for name, _power_w, _temp_k in entries]

        if len(seen) != len(set(seen)):
            raise ValueError(f"Duplicate HotSpot runtime labels: {seen}")

        seen_set = set(seen)

        missing = self._expected_label_set - seen_set
        extra = seen_set - self._expected_label_set

        if missing or extra:
            raise ValueError(
                f"HotSpot domain mismatch: missing={sorted(missing)}, "
                f"extra={sorted(extra)}"
            )

        power = {}
        for name, power_w, current_temp_k in entries:
            power_w = float(power_w)
            current_temp_k = float(current_temp_k)

            if not math.isfinite(power_w) or power_w < 0:
                raise ValueError(
                    f"Invalid sampled power for {name}: {power_w}"
                )

            if not math.isfinite(current_temp_k) or current_temp_k <= 0:
                raise ValueError(
                    f"Invalid input temperature for {name}: {current_temp_k}"
                )

            power[name] = power_w

        nsteps = max(1, math.ceil(dt_seconds / self._max_external_step_s))
        sub_dt_s = dt_seconds / nsteps

        temps = None
        for _ in range(nsteps):
            temps = self._solver.step(
                power, dt_s=sub_dt_s, method=self._method
            )

        if not isinstance(temps, dict):
            raise TypeError("HotSpot solver must return dict[label] = temp_K")

        missing = self._expected_label_set - set(temps)
        extra = set(temps) - self._expected_label_set
        if missing or extra:
            raise ValueError(
                f"HotSpot result mismatch: missing={sorted(missing)}, "
                f"extra={sorted(extra)}"
            )

        for label, temp_k in temps.items():
            if not math.isfinite(float(temp_k)) or float(temp_k) <= 0:
                raise ValueError(f"Invalid temperature for {label}: {temp_k}")

        return temps, nsteps
