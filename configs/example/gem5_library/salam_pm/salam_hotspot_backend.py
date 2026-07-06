"""HotSpot floorplan backend for SALAM per-block thermal domains."""

from __future__ import annotations

import json
import math
from pathlib import Path

from thermal_backends.hotspot_runtime_adapter import HotSpotRuntimeAdapter

FLOORPLAN_LOG_NAME = "salam_hotspot_floorplan.json"

BLOCK_GEOMETRY = {
    "datapath": (0.0030, 0.0030),
    "registers": (0.0010, 0.0010),
    "spm": (0.0020, 0.0020),
}

BLOCK_KIND_ORDER = {"datapath": 0, "registers": 1, "spm": 2}

BLOCK_KIND_AREA_STAT = {
    "datapath": "power.fuAreaUm2",
    "registers": "power.regAreaUm2",
    "spm": "power.spmAreaUm2",
}

ROW_TARGET_WIDTH_M = 0.006
MIN_BLOCK_M = 0.0005


def _block_kind(label: str) -> str:
    kind = label.rsplit(".", 1)[-1]
    if kind not in BLOCK_GEOMETRY:
        raise ValueError(f"Unsupported SALAM thermal block in label: {label}")
    return kind


def _read_scalar_stat(stat_source, stat_name: str) -> float:
    stat_info = stat_source.resolveStat(stat_name)
    if not stat_info:
        return 0.0
    stat_info.prepare()
    return float(stat_info.total)


def read_block_areas(stat_source) -> dict[str, float]:
    return {
        kind: _read_scalar_stat(stat_source, stat_path)
        for kind, stat_path in BLOCK_KIND_AREA_STAT.items()
    }


def areas_ready(stat_source) -> bool:
    return _read_scalar_stat(stat_source, "power.areasReady") >= 1.0


def _block_sizes_from_areas(block_areas_um2: dict[str, float]):
    total_area = sum(block_areas_um2.values())
    if total_area <= 0.0:
        return None

    sizes = {}
    for label, area_um2 in block_areas_um2.items():
        if area_um2 <= 0.0:
            kind = _block_kind(label)
            sizes[label] = BLOCK_GEOMETRY[kind]
            continue

        frac = area_um2 / total_area
        width_m = max(MIN_BLOCK_M, frac * ROW_TARGET_WIDTH_M)
        height_m = width_m
        sizes[label] = (width_m, height_m)
    return sizes


def build_salam_hotspot_floorplan(labels, block_areas_um2=None):
    """Lay out one HotSpot block per thermal-domain label."""
    labels = list(labels)
    if not labels:
        raise ValueError("Cannot build SALAM HotSpot floorplan with no labels")

    by_accel = {}
    for label in labels:
        prefix = label.rsplit(".", 1)[0]
        by_accel.setdefault(prefix, []).append(label)

    floorplan = []
    y_cursor = 0.0
    max_width = 0.0

    for _prefix, group in sorted(by_accel.items()):
        ordered = sorted(group, key=_block_kind)
        row_areas = None
        if block_areas_um2 is not None:
            row_areas = {
                label: block_areas_um2.get(label, 0.0) for label in ordered
            }
        row_sizes = (
            _block_sizes_from_areas(row_areas)
            if row_areas is not None
            else None
        )

        x_cursor = 0.0
        row_height = 0.0
        for label in ordered:
            kind = _block_kind(label)
            if row_sizes and label in row_sizes:
                width_m, height_m = row_sizes[label]
            else:
                width_m, height_m = BLOCK_GEOMETRY[kind]
            floorplan.append(
                {
                    "name": label,
                    "width_m": width_m,
                    "height_m": height_m,
                    "x_m": x_cursor,
                    "y_m": y_cursor,
                }
            )
            x_cursor += width_m
            row_height = max(row_height, height_m)
        max_width = max(max_width, x_cursor)
        y_cursor += row_height

    return floorplan, max_width, y_cursor


def _hotspot_layers():
    return [
        {
            "has_lateral": True,
            "has_power": True,
            "k_W_mK": 100.0,
            "thickness_m": 0.15e-3,
            "vol_heat_cap_J_m3K": 1.75e6,
        },
        {
            "has_lateral": True,
            "has_power": False,
            "k_W_mK": 4.0,
            "thickness_m": 20e-6,
            "vol_heat_cap_J_m3K": 4.0e6,
        },
    ]


def _hotspot_config(args, chip_width_m, chip_height_m):
    return {
        "model_type": args.hotspot_model_type,
        "chip_width_m": max(chip_width_m, 0.004),
        "chip_height_m": max(chip_height_m, 0.004),
        "grid_rows": args.hotspot_grid_rows,
        "grid_cols": args.hotspot_grid_cols,
        "spreader_width_m": 0.03,
        "spreader_height_m": 0.03,
        "spreader_thickness_m": 1e-3,
        "spreader_k_W_mK": 400.0,
        "spreader_vol_heat_cap_J_m3K": 3.55e6,
        "heatsink_width_m": 0.06,
        "heatsink_height_m": 0.06,
        "heatsink_thickness_m": 6.9e-3,
        "heatsink_k_W_mK": 400.0,
        "heatsink_vol_heat_cap_J_m3K": 3.55e6,
        "r_convec_K_per_W": 0.1,
        "c_convec_J_per_K": 140.4,
        "ambient_temp_K": args.hotspot_ambient_temp_k,
        "initial_temp_K": args.hotspot_initial_temp_k,
        "model_secondary": args.hotspot_model_secondary,
        "detailed_3D_used": False,
        "grid_map_mode": "center",
    }


def build_salam_hotspot_backend(labels, args, block_areas_um2=None):
    floorplan, chip_width_m, chip_height_m = build_salam_hotspot_floorplan(
        labels, block_areas_um2=block_areas_um2
    )

    return HotSpotRuntimeAdapter(
        floorplan=floorplan,
        layers=_hotspot_layers(),
        config=_hotspot_config(args, chip_width_m, chip_height_m),
        method=args.hotspot_method,
        max_external_step_s=args.hotspot_max_external_step_s,
    )


def write_floorplan_log(
    floorplan,
    chip_width_m,
    chip_height_m,
    block_areas_um2=None,
    geometry_mode="area",
    passthrough_steps=0,
    outdir=None,
):
    """Persist the autogenerated HotSpot floorplan for post-run inspection."""
    import m5

    if outdir is None:
        outdir = m5.options.outdir
    path = Path(outdir) / FLOORPLAN_LOG_NAME
    payload = {
        "geometry_mode": geometry_mode,
        "passthrough_steps_before_build": passthrough_steps,
        "chip_width_m": chip_width_m,
        "chip_height_m": chip_height_m,
        "block_areas_um2": block_areas_um2 or {},
        "blocks": floorplan,
    }
    path.write_text(json.dumps(payload, indent=2) + "\n")
    return path


def _collect_block_areas_um2(labels, label_to_source):
    block_areas = {}
    for label in labels:
        stat_source = label_to_source[label]
        areas_by_kind = read_block_areas(stat_source)
        kind = _block_kind(label)
        block_areas[label] = areas_by_kind.get(kind, 0.0)
    return block_areas


def _all_sources_ready(label_to_source):
    seen = set()
    for stat_source in label_to_source.values():
        source_id = id(stat_source)
        if source_id in seen:
            continue
        seen.add(source_id)
        if not areas_ready(stat_source):
            return False
    return bool(seen)


class LazySalamHotSpotAdapter:
    """Build HotSpot once on the first thermal step when block areas exist."""

    def __init__(
        self,
        labels,
        label_to_source,
        args,
        geometry_mode="area",
        trace_debug=False,
    ):
        self._labels = list(labels)
        self._label_to_source = dict(label_to_source)
        self._args = args
        self._geometry_mode = geometry_mode
        self._trace_debug = trace_debug
        self._backend = None
        self._pending_reset_temp_k = None
        self._last_substeps = 1
        self._passthrough_steps = 0
        self._floorplan_log_path = None

    def _trace(self, msg: str):
        if self._trace_debug:
            print(msg)

    def _default_reset_temp_k(self):
        return float(getattr(self._args, "hotspot_initial_temp_k", 300.0))

    def _sanitize_reset_temp_k(self, initial_temp_k, domain_data=None):
        temp_k = float(initial_temp_k)
        if math.isfinite(temp_k) and temp_k > 0.0:
            return temp_k

        if domain_data is not None:
            for _label, _power_w, current_temp_k in domain_data:
                candidate = float(current_temp_k)
                if math.isfinite(candidate) and candidate > 0.0:
                    return candidate

        return self._default_reset_temp_k()

    def _ensure_backend(self, domain_data=None):
        if self._backend is not None:
            return True

        block_areas_um2 = None
        if self._geometry_mode == "area":
            if not _all_sources_ready(self._label_to_source):
                return False
            block_areas_um2 = _collect_block_areas_um2(
                self._labels, self._label_to_source
            )
            if not any(area > 0.0 for area in block_areas_um2.values()):
                return False

        self._backend = build_salam_hotspot_backend(
            self._labels,
            self._args,
            block_areas_um2=block_areas_um2,
        )
        floorplan, chip_w, chip_h = build_salam_hotspot_floorplan(
            self._labels, block_areas_um2=block_areas_um2
        )
        self._floorplan_log_path = write_floorplan_log(
            floorplan,
            chip_w,
            chip_h,
            block_areas_um2=block_areas_um2,
            geometry_mode=self._geometry_mode,
            passthrough_steps=self._passthrough_steps,
        )
        reset_temp_k = self._sanitize_reset_temp_k(
            (
                self._pending_reset_temp_k
                if self._pending_reset_temp_k is not None
                else self._default_reset_temp_k()
            ),
            domain_data=domain_data,
        )
        self._pending_reset_temp_k = reset_temp_k
        self._backend.reset(reset_temp_k)
        self._trace(
            "LazySalamHotSpotAdapter: built area-proportional HotSpot floorplan "
            f"({self._floorplan_log_path})"
        )
        return True

    def reset(self, initial_temp_k):
        reset_temp_k = self._sanitize_reset_temp_k(initial_temp_k)
        self._pending_reset_temp_k = reset_temp_k
        if self._backend is not None:
            self._backend.reset(reset_temp_k)

    def solve(self, domain_data, dt_seconds):
        if not self._ensure_backend(domain_data=domain_data):
            self._passthrough_steps += 1
            temps = {
                label: float(temp_k) for label, _power_w, temp_k in domain_data
            }
            self._last_substeps = 1
            return temps, self._last_substeps

        result = self._backend.solve(domain_data, dt_seconds)
        if isinstance(result, tuple):
            temps, self._last_substeps = result
            return temps, self._last_substeps

        self._last_substeps = 1
        return result, self._last_substeps

    def getLastSolverSubsteps(self):
        if self._backend is not None and hasattr(
            self._backend, "getLastSolverSubsteps"
        ):
            return self._backend.getLastSolverSubsteps()
        return self._last_substeps


def build_lazy_salam_hotspot_backend(
    labels, bindings, args, geometry_mode="area", trace_debug=False
):
    label_to_source = {
        label: stat_source for label, stat_source, _pm in bindings
    }
    return LazySalamHotSpotAdapter(
        labels,
        label_to_source,
        args,
        geometry_mode=geometry_mode,
        trace_debug=trace_debug,
    )
