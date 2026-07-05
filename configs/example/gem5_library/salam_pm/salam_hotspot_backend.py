"""HotSpot floorplan backend for SALAM per-block thermal domains."""

from __future__ import annotations

from thermal_backends.hotspot_runtime_adapter import HotSpotRuntimeAdapter

BLOCK_GEOMETRY = {
    "datapath": (0.0030, 0.0030),
    "registers": (0.0010, 0.0010),
    "spm": (0.0020, 0.0020),
}


def _block_kind(label: str) -> str:
    kind = label.rsplit(".", 1)[-1]
    if kind not in BLOCK_GEOMETRY:
        raise ValueError(f"Unsupported SALAM thermal block in label: {label}")
    return kind


def build_salam_hotspot_floorplan(labels):
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
        x_cursor = 0.0
        row_height = 0.0
        for label in ordered:
            kind = _block_kind(label)
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


def build_salam_hotspot_backend(labels, args):
    floorplan, chip_width_m, chip_height_m = build_salam_hotspot_floorplan(
        labels
    )

    layers = [
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

    config = {
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

    return HotSpotRuntimeAdapter(
        floorplan=floorplan,
        layers=layers,
        config=config,
        method=args.hotspot_method,
        max_external_step_s=args.hotspot_max_external_step_s,
    )
