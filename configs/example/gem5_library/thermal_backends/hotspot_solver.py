from __future__ import annotations

import math
from dataclasses import (
    dataclass,
    field,
)
from typing import Optional

import numpy as np
from scipy import sparse
from scipy.sparse.linalg import spsolve

# Constants from temperature.h, util.h, RCutil.c
EXTRA = 12
EXTRA_SEC = 16

# Spreader peripheral nodes (temperature.h)
SP_W = 0
SP_E = 1
SP_N = 2
SP_S = 3
# Central sink nodes (directly under the spreader)
SINK_C_W = 4
SINK_C_E = 5
SINK_C_N = 6
SINK_C_S = 7
# Peripheral sink nodes
SINK_W = 8
SINK_E = 9
SINK_N = 10
SINK_S = 11
# Package substrate nodes
SUB_W = 12
SUB_E = 13
SUB_N = 14
SUB_S = 15
# Solder ball nodes
SOLDER_W = 16
SOLDER_E = 17
SOLDER_N = 18
SOLDER_S = 19
# Central PCB nodes (directly under the solder balls)
PCB_C_W = 20
PCB_C_E = 21
PCB_C_N = 22
PCB_C_S = 23
# Peripheral PCB nodes
PCB_W = 24
PCB_E = 25
PCB_N = 26
PCB_S = 27

# Grid map modes (temperature.h)
GRID_AVG = 0
GRID_MIN = 1
GRID_MAX = 2
GRID_CENTER = 3

# Model specific constants (temperature.h, RCutil.c)
C_FACTOR = 0.333
MIN_STEP = 1e-7

# RK4 constants (RCutil.c)
RK4_SAFETY = 0.95
RK4_MAXUP = 5.0
RK4_MAXDOWN = 10.0
RK4_PRECISION = 0.01

# Utility constants (util.h)
DELTA = 1.0e-6
LARGENUM = 1.0e100
OCCUPANCY_THRESHOLD = 0.95

# Secondary path material properties (temperature.h)
RHO_SUB = 0.5
K_SUB = 1.0 / RHO_SUB
RHO_SOLDER = 0.06
K_SOLDER = 1.0 / RHO_SOLDER
RHO_PCB = 0.333
K_PCB = 1.0 / RHO_PCB
RHO_METAL = 0.0025
K_METAL = 1.0 / RHO_METAL
RHO_C4 = 0.8
K_C4 = 1.0 / RHO_C4

SPEC_HEAT_SUB = 1.6e6
SPEC_HEAT_SOLDER = 2.1e6
SPEC_HEAT_PCB = 1.32e6
SPEC_HEAT_METAL = 3.55e6
SPEC_HEAT_C4 = 1.65e6

# Default layer indices (temperature_grid.h)
DEFAULT_CHIP_LAYERS = 2
DEFAULT_PACK_LAYERS = 2
SEC_CHIP_LAYERS = 2
SEC_PACK_LAYERS = 3
LAYER_SP = 0
LAYER_SINK = 1  # package layer offsets
LAYER_SI = 0
LAYER_INT = 1  # default chip layer offsets
LAYER_C4_SEC = 0
LAYER_METAL = 1  # secondary chip layer offsets
LAYER_PCB_SEC = 0
LAYER_SOLDER_SEC = 1
LAYER_SUB_SEC = 2  # secondary package

# Block model layer indices (temperature_block.h)
NL = 4
IFACE = 1
HSP = 2
HSINK = 3


# Helper functions from RCutil.c and util.c
def getr(conductivity: float, thickness: float, area: float) -> float:
    """Thermal resistance. Implements getr() from RCutil.c."""
    return thickness / (conductivity * area)


def getcap(sp_heat: float, thickness: float, area: float) -> float:
    """Thermal capacitance with lumped correction. Implements getcap() from RCutil.c."""
    return C_FACTOR * sp_heat * thickness * area


def eq(x: float, y: float) -> bool:
    """Floating-point equality within DELTA tolerance. From util.c."""
    return abs(x - y) < DELTA


def tolerant_ceil(val: float) -> int:
    """Ceil with tolerance for near-integers. From util.c."""
    nearest = math.floor(val + 0.5)
    if abs(val - nearest) < DELTA:
        return int(nearest)
    return math.ceil(val)


def tolerant_floor(val: float) -> int:
    """Floor with tolerance for near-integers. From util.c."""
    nearest = math.floor(val + 0.5)
    if abs(val - nearest) < DELTA:
        return int(nearest)
    return math.floor(val)


# Data structures
@dataclass
class FlpUnit:
    """One floorplan block. Mirrors unit_t from flp.h."""

    name: str
    width: float
    height: float
    leftx: float
    bottomy: float
    specificheat: float = 0.0  # J/(m^3-K), 0 = use layer default
    resistivity: float = 0.0  # m-K/W, 0 = use layer default
    hasRes: bool = False
    hasSh: bool = False


@dataclass
class PackageRC:
    """All package RC scalars. Mirrors package_RC_t from temperature.h."""

    # Primary path - lateral R
    r_sp1_x: float = 0.0
    r_sp1_y: float = 0.0
    r_hs1_x: float = 0.0
    r_hs1_y: float = 0.0
    r_hs2_x: float = 0.0
    r_hs2_y: float = 0.0
    r_hs: float = 0.0
    # Primary path - vertical R
    r_sp_per_x: float = 0.0
    r_sp_per_y: float = 0.0
    r_hs_c_per_x: float = 0.0
    r_hs_c_per_y: float = 0.0
    r_hs_per: float = 0.0
    # Primary path - vertical C
    c_sp_per_x: float = 0.0
    c_sp_per_y: float = 0.0
    c_hs_c_per_x: float = 0.0
    c_hs_c_per_y: float = 0.0
    c_hs_per: float = 0.0
    # Primary path - ambient R and C
    r_amb_c_per_x: float = 0.0
    c_amb_c_per_x: float = 0.0
    r_amb_c_per_y: float = 0.0
    c_amb_c_per_y: float = 0.0
    r_amb_per: float = 0.0
    c_amb_per: float = 0.0
    # Secondary path - lateral R
    r_sub1_x: float = 0.0
    r_sub1_y: float = 0.0
    r_solder1_x: float = 0.0
    r_solder1_y: float = 0.0
    r_pcb1_x: float = 0.0
    r_pcb1_y: float = 0.0
    r_pcb2_x: float = 0.0
    r_pcb2_y: float = 0.0
    r_pcb: float = 0.0
    # Secondary path - vertical R
    r_sub_per_x: float = 0.0
    r_sub_per_y: float = 0.0
    r_solder_per_x: float = 0.0
    r_solder_per_y: float = 0.0
    r_pcb_c_per_x: float = 0.0
    r_pcb_c_per_y: float = 0.0
    r_pcb_per: float = 0.0
    # Secondary path - vertical C
    c_sub_per_x: float = 0.0
    c_sub_per_y: float = 0.0
    c_solder_per_x: float = 0.0
    c_solder_per_y: float = 0.0
    c_pcb_c_per_x: float = 0.0
    c_pcb_c_per_y: float = 0.0
    c_pcb_per: float = 0.0
    # Secondary path - ambient R and C
    r_amb_sec_c_per_x: float = 0.0
    c_amb_sec_c_per_x: float = 0.0
    r_amb_sec_c_per_y: float = 0.0
    c_amb_sec_c_per_y: float = 0.0
    r_amb_sec_per: float = 0.0
    c_amb_sec_per: float = 0.0


@dataclass
class Layer:
    """Per-layer geometry and mapping. Mirrors layer_t from temperature_grid.h."""

    no: int = 0
    has_lateral: bool = True
    has_power: bool = False
    k: float = 0.0
    thickness: float = 0.0
    sp: float = 0.0
    rx: float = 0.0
    ry: float = 0.0
    rz: float = 0.0
    c: float = 0.0
    flp: list = field(default_factory=list)
    b2gmap: list | None = None
    g2bmap: list | None = None
    is_microchannel: bool = False
    mc_config: object = None  # MicrochannelConfig when is_microchannel
    cell_rx: object = None  # np.ndarray [nr,nc] for detailed_3D
    cell_ry: object = None
    cell_rz: object = None
    cell_cap: object = None
    has_custom_rc: object = None  # np.ndarray bool [nr,nc]


# Microchannel constants and model — from microchannel.h / microchannel.c
MC_TSV = -1
MC_SOLID = 0
MC_FLUID = 1
MC_INLET = 2
MC_OUTLET = 3


def _is_fluid_cell(cell_types, i, j):
    return cell_types[i, j] in (MC_FLUID, MC_INLET, MC_OUTLET)


@dataclass
class MicrochannelConfig:
    """Microchannel layer configuration. Mirrors microchannel_config_t."""

    cell_types: object = None  # np.ndarray [nr, nc] int
    pumping_pressure: float = 5000.0
    pump_internal_res: float = 0.0
    inlet_temperature: float = 300.0
    coolant_capac: float = 4172638.0
    coolant_res: float = 1.647717911
    coolant_visc: float = 0.000889
    wall_capac: float = 1635660.0
    wall_res: float = 0.0076923077
    htc: float = 27132.0
    cell_width: float = 100e-6
    cell_height: float = 100e-6
    cell_thickness: float = 100e-6
    pressures: object = None  # np.ndarray after solve
    n_fluid: int = 0
    mapping: object = None  # np.ndarray int [nr, nc] -> pressure node index


def _hydro_conductance(mc: MicrochannelConfig) -> float:
    """Hydraulic conductance of one duct segment. Implements hydroC() from microchannel.c."""
    h = mc.cell_thickness
    w = mc.cell_width
    L = mc.cell_height
    mu = mc.coolant_visc
    if abs(h - w) < 1e-30:
        return 0.42229 * h**4 / (12.0 * mu * L)
    elif h > w:
        return (1.0 - 0.63 * (w / h)) * w**3 * h / (12.0 * mu * L)
    else:
        return (1.0 - 0.63 * (h / w)) * h**3 * w / (12.0 * mu * L)


def _build_pressure_network(mc: MicrochannelConfig):
    """Build and solve pressure network for microchannel. Implements
    build_pressure_matrix() + solve_pressure_circuit() from microchannel.c."""
    ct = mc.cell_types
    nr, nc = ct.shape

    mapping = np.full((nr, nc), -1, dtype=int)
    idx = 0
    for i in range(nr):
        for j in range(nc):
            if _is_fluid_cell(ct, i, j):
                mapping[i, j] = idx
                idx += 1
    mc.n_fluid = idx
    mc.mapping = mapping

    G_h = _hydro_conductance(mc)

    extra_node = idx if mc.pump_internal_res > 0 else -1
    n_eq = idx + (1 if extra_node >= 0 else 0)
    A = np.zeros((n_eq, n_eq))
    b = np.zeros(n_eq)

    neighbors = [(-1, 0), (1, 0), (0, -1), (0, 1)]
    for i in range(nr):
        for j in range(nc):
            if not _is_fluid_cell(ct, i, j):
                continue
            node = mapping[i, j]
            ctype = ct[i, j]

            if mc.pump_internal_res == 0:
                if ctype == MC_INLET:
                    A[node, node] = 1.0
                    b[node] = mc.pumping_pressure
                    continue
                elif ctype == MC_OUTLET:
                    A[node, node] = 1.0
                    b[node] = 0.0
                    continue
            else:
                if ctype == MC_INLET:
                    inv_Rp = 1.0 / mc.pump_internal_res
                    A[node, node] += inv_Rp
                    A[node, extra_node] -= inv_Rp
                    A[extra_node, node] -= inv_Rp
                    A[extra_node, extra_node] += inv_Rp
                elif ctype == MC_OUTLET:
                    A[node, node] = 1.0
                    b[node] = 0.0
                    continue

            for di, dj in neighbors:
                ni, nj = i + di, j + dj
                if (
                    0 <= ni < nr
                    and 0 <= nj < nc
                    and _is_fluid_cell(ct, ni, nj)
                ):
                    nb = mapping[ni, nj]
                    A[node, node] += G_h
                    A[node, nb] -= G_h

    if extra_node >= 0:
        A[extra_node, extra_node] += 0.0
        b[extra_node] = mc.pumping_pressure

    pressures_all = np.linalg.solve(A, b)
    pres_grid = np.zeros((nr, nc))
    for i in range(nr):
        for j in range(nc):
            if mapping[i, j] >= 0:
                pres_grid[i, j] = pressures_all[mapping[i, j]]
    mc.pressures = pres_grid


def _mc_flow_rate(
    mc: MicrochannelConfig, i1: int, j1: int, i2: int, j2: int
) -> float:
    """Flow rate between adjacent fluid cells. Implements flow_rate() from microchannel.c."""
    if mc.pressures is None:
        return 0.0
    G_h = _hydro_conductance(mc)
    return (mc.pressures[i1, j1] - mc.pressures[i2, j2]) * G_h


def _build_mc_synthetic_flp(mc: MicrochannelConfig, nr: int, nc: int) -> list:
    """Generate synthetic FlpUnit list for microchannel layer.
    Implements the floorplan generation in microchannel_build_network()."""
    ct = mc.cell_types
    cw = mc.cell_width
    ch = mc.cell_height
    units = []
    for i in range(nr):
        for j in range(nc):
            is_fluid = _is_fluid_cell(ct, i, j)
            sp = mc.coolant_capac if is_fluid else mc.wall_capac
            res = mc.coolant_res if is_fluid else mc.wall_res
            units.append(
                FlpUnit(
                    name=f"Cell_{i}_{j}",
                    width=cw,
                    height=ch,
                    leftx=j * cw,
                    bottomy=(nr - i - 1) * ch,
                    specificheat=sp,
                    resistivity=res,
                    hasRes=True,
                    hasSh=True,
                )
            )
    return units


# Package RC construction — populate_package_R/C from temperature.c
def populate_package_R(pk: PackageRC, cfg: dict, width: float, height: float):
    """Implements populate_package_R() from temperature.c."""
    s_sp = cfg["s_spreader"]
    t_sp = cfg["t_spreader"]
    s_sk = cfg["s_sink"]
    t_sk = cfg["t_sink"]
    r_cv = cfg["r_convec"]
    k_sk = cfg["k_sink"]
    k_sp = cfg["k_spreader"]
    s_sub = cfg["s_sub"]
    t_sub = cfg["t_sub"]
    s_sol = cfg["s_solder"]
    t_sol = cfg["t_solder"]
    s_pcb = cfg["s_pcb"]
    t_pcb = cfg["t_pcb"]
    r_cv_sec = cfg.get("r_convec_sec", 1.0)

    pk.r_sp1_x = getr(
        k_sp, (s_sp - width) / 4.0, (s_sp + 3 * height) / 4.0 * t_sp
    )
    pk.r_sp1_y = getr(
        k_sp, (s_sp - height) / 4.0, (s_sp + 3 * width) / 4.0 * t_sp
    )
    pk.r_hs1_x = getr(
        k_sk, (s_sp - width) / 4.0, (s_sp + 3 * height) / 4.0 * t_sk
    )
    pk.r_hs1_y = getr(
        k_sk, (s_sp - height) / 4.0, (s_sp + 3 * width) / 4.0 * t_sk
    )
    pk.r_hs2_x = getr(
        k_sk, (s_sp - width) / 4.0, (3 * s_sp + height) / 4.0 * t_sk
    )
    pk.r_hs2_y = getr(
        k_sk, (s_sp - height) / 4.0, (3 * s_sp + width) / 4.0 * t_sk
    )
    pk.r_hs = getr(k_sk, (s_sk - s_sp) / 4.0, (s_sk + 3 * s_sp) / 4.0 * t_sk)

    pk.r_sp_per_x = getr(k_sp, t_sp, (s_sp + height) * (s_sp - width) / 4.0)
    pk.r_sp_per_y = getr(k_sp, t_sp, (s_sp + width) * (s_sp - height) / 4.0)
    pk.r_hs_c_per_x = getr(k_sk, t_sk, (s_sp + height) * (s_sp - width) / 4.0)
    pk.r_hs_c_per_y = getr(k_sk, t_sk, (s_sp + width) * (s_sp - height) / 4.0)
    pk.r_hs_per = getr(k_sk, t_sk, (s_sk * s_sk - s_sp * s_sp) / 4.0)

    pk.r_amb_c_per_x = (
        r_cv * (s_sk * s_sk) / ((s_sp + height) * (s_sp - width) / 4.0)
    )
    pk.r_amb_c_per_y = (
        r_cv * (s_sk * s_sk) / ((s_sp + width) * (s_sp - height) / 4.0)
    )
    pk.r_amb_per = r_cv * (s_sk * s_sk) / ((s_sk * s_sk - s_sp * s_sp) / 4.0)

    # Secondary path R
    pk.r_sub1_x = getr(
        K_SUB, (s_sub - width) / 4.0, (s_sub + 3 * height) / 4.0 * t_sub
    )
    pk.r_sub1_y = getr(
        K_SUB, (s_sub - height) / 4.0, (s_sub + 3 * width) / 4.0 * t_sub
    )
    pk.r_solder1_x = getr(
        K_SOLDER, (s_sol - width) / 4.0, (s_sol + 3 * height) / 4.0 * t_sol
    )
    pk.r_solder1_y = getr(
        K_SOLDER, (s_sol - height) / 4.0, (s_sol + 3 * width) / 4.0 * t_sol
    )
    pk.r_pcb1_x = getr(
        K_PCB, (s_sol - width) / 4.0, (s_sol + 3 * height) / 4.0 * t_pcb
    )
    pk.r_pcb1_y = getr(
        K_PCB, (s_sol - height) / 4.0, (s_sol + 3 * width) / 4.0 * t_pcb
    )
    pk.r_pcb2_x = getr(
        K_PCB, (s_sol - width) / 4.0, (3 * s_sol + height) / 4.0 * t_pcb
    )
    pk.r_pcb2_y = getr(
        K_PCB, (s_sol - height) / 4.0, (3 * s_sol + width) / 4.0 * t_pcb
    )
    pk.r_pcb = getr(
        K_PCB, (s_pcb - s_sol) / 4.0, (s_pcb + 3 * s_sol) / 4.0 * t_pcb
    )

    pk.r_sub_per_x = getr(
        K_SUB, t_sub, (s_sub + height) * (s_sub - width) / 4.0
    )
    pk.r_sub_per_y = getr(
        K_SUB, t_sub, (s_sub + width) * (s_sub - height) / 4.0
    )
    pk.r_solder_per_x = getr(
        K_SOLDER, t_sol, (s_sol + height) * (s_sol - width) / 4.0
    )
    pk.r_solder_per_y = getr(
        K_SOLDER, t_sol, (s_sol + width) * (s_sol - height) / 4.0
    )
    pk.r_pcb_c_per_x = getr(
        K_PCB, t_pcb, (s_sol + height) * (s_sol - width) / 4.0
    )
    pk.r_pcb_c_per_y = getr(
        K_PCB, t_pcb, (s_sol + width) * (s_sol - height) / 4.0
    )
    pk.r_pcb_per = getr(K_PCB, t_pcb, (s_pcb * s_pcb - s_sol * s_sol) / 4.0)

    pk.r_amb_sec_c_per_x = (
        r_cv_sec * (s_pcb * s_pcb) / ((s_sol + height) * (s_sol - width) / 4.0)
    )
    pk.r_amb_sec_c_per_y = (
        r_cv_sec * (s_pcb * s_pcb) / ((s_sol + width) * (s_sol - height) / 4.0)
    )
    pk.r_amb_sec_per = (
        r_cv_sec * (s_pcb * s_pcb) / ((s_pcb * s_pcb - s_sol * s_sol) / 4.0)
    )


def populate_package_C(pk: PackageRC, cfg: dict, width: float, height: float):
    """Implements populate_package_C() from temperature.c."""
    s_sp = cfg["s_spreader"]
    t_sp = cfg["t_spreader"]
    s_sk = cfg["s_sink"]
    t_sk = cfg["t_sink"]
    c_cv = cfg["c_convec"]
    p_sk = cfg["p_sink"]
    p_sp = cfg["p_spreader"]
    s_sub = cfg["s_sub"]
    t_sub = cfg["t_sub"]
    s_sol = cfg["s_solder"]
    t_sol = cfg["t_solder"]
    s_pcb = cfg["s_pcb"]
    t_pcb = cfg["t_pcb"]
    c_cv_sec = cfg.get("c_convec_sec", 140.4)

    pk.c_sp_per_x = getcap(p_sp, t_sp, (s_sp + height) * (s_sp - width) / 4.0)
    pk.c_sp_per_y = getcap(p_sp, t_sp, (s_sp + width) * (s_sp - height) / 4.0)
    pk.c_hs_c_per_x = getcap(
        p_sk, t_sk, (s_sp + height) * (s_sp - width) / 4.0
    )
    pk.c_hs_c_per_y = getcap(
        p_sk, t_sk, (s_sp + width) * (s_sp - height) / 4.0
    )
    pk.c_hs_per = getcap(p_sk, t_sk, (s_sk * s_sk - s_sp * s_sp) / 4.0)

    pk.c_amb_c_per_x = (
        C_FACTOR
        * c_cv
        / (s_sk * s_sk)
        * ((s_sp + height) * (s_sp - width) / 4.0)
    )
    pk.c_amb_c_per_y = (
        C_FACTOR
        * c_cv
        / (s_sk * s_sk)
        * ((s_sp + width) * (s_sp - height) / 4.0)
    )
    pk.c_amb_per = (
        C_FACTOR * c_cv / (s_sk * s_sk) * ((s_sk * s_sk - s_sp * s_sp) / 4.0)
    )

    # Secondary path C
    pk.c_sub_per_x = getcap(
        SPEC_HEAT_SUB, t_sub, (s_sub + height) * (s_sub - width) / 4.0
    )
    pk.c_sub_per_y = getcap(
        SPEC_HEAT_SUB, t_sub, (s_sub + width) * (s_sub - height) / 4.0
    )
    pk.c_solder_per_x = getcap(
        SPEC_HEAT_SOLDER, t_sol, (s_sol + height) * (s_sol - width) / 4.0
    )
    pk.c_solder_per_y = getcap(
        SPEC_HEAT_SOLDER, t_sol, (s_sol + width) * (s_sol - height) / 4.0
    )
    pk.c_pcb_c_per_x = getcap(
        SPEC_HEAT_PCB, t_pcb, (s_sol + height) * (s_sol - width) / 4.0
    )
    pk.c_pcb_c_per_y = getcap(
        SPEC_HEAT_PCB, t_pcb, (s_sol + width) * (s_sol - height) / 4.0
    )
    pk.c_pcb_per = getcap(
        SPEC_HEAT_PCB, t_pcb, (s_pcb * s_pcb - s_sol * s_sol) / 4.0
    )

    pk.c_amb_sec_c_per_x = (
        C_FACTOR
        * c_cv_sec
        / (s_pcb * s_pcb)
        * ((s_sol + height) * (s_sol - width) / 4.0)
    )
    pk.c_amb_sec_c_per_y = (
        C_FACTOR
        * c_cv_sec
        / (s_pcb * s_pcb)
        * ((s_sol + width) * (s_sol - height) / 4.0)
    )
    pk.c_amb_sec_per = (
        C_FACTOR
        * c_cv_sec
        / (s_pcb * s_pcb)
        * ((s_pcb * s_pcb - s_sol * s_sol) / 4.0)
    )


# Floorplan adjacency helpers — from flp.c
def _get_total_width(units: list[FlpUnit]) -> float:
    if not units:
        return 0.0
    return max(u.leftx + u.width for u in units) - min(u.leftx for u in units)


def _get_total_height(units: list[FlpUnit]) -> float:
    if not units:
        return 0.0
    return max(u.bottomy + u.height for u in units) - min(
        u.bottomy for u in units
    )


def _is_horiz_adj(flp: list[FlpUnit], i: int, j: int) -> bool:
    """True if blocks i and j share a vertical edge (horizontally adjacent)."""
    a, b = flp[i], flp[j]
    return (
        eq(a.leftx + a.width, b.leftx) or eq(b.leftx + b.width, a.leftx)
    ) and not (
        a.bottomy + a.height <= b.bottomy + DELTA
        or b.bottomy + b.height <= a.bottomy + DELTA
    )


def _is_vert_adj(flp: list[FlpUnit], i: int, j: int) -> bool:
    """True if blocks i and j share a horizontal edge (vertically adjacent)."""
    a, b = flp[i], flp[j]
    return (
        eq(a.bottomy + a.height, b.bottomy)
        or eq(b.bottomy + b.height, a.bottomy)
    ) and not (
        a.leftx + a.width <= b.leftx + DELTA
        or b.leftx + b.width <= a.leftx + DELTA
    )


def _get_shared_len(flp: list[FlpUnit], i: int, j: int) -> float:
    """Length of shared edge between blocks i and j. From flp.c."""
    a, b = flp[i], flp[j]
    if _is_horiz_adj(flp, i, j):
        ymin = max(a.bottomy, b.bottomy)
        ymax = min(a.bottomy + a.height, b.bottomy + b.height)
        return max(0.0, ymax - ymin)
    if _is_vert_adj(flp, i, j):
        xmin = max(a.leftx, b.leftx)
        xmax = min(a.leftx + a.width, b.leftx + b.width)
        return max(0.0, xmax - xmin)
    return 0.0


# ---------------------------------------------------------------------------
# Grid Model
# ---------------------------------------------------------------------------
class _GridModel:
    """Internal grid model. Implements grid_model_t from temperature_grid.c."""

    def __init__(
        self, flp_units: list[FlpUnit], layers_cfg: list[dict], cfg: dict
    ):
        self.cfg = cfg
        self.nr = cfg.get("grid_rows", 64)
        self.nc = cfg.get("grid_cols", 64)
        self.model_secondary = cfg.get("model_secondary", False)
        self.detailed_3D = cfg.get("detailed_3D_used", False)
        map_str = cfg.get("grid_map_mode", "center")
        self.map_mode = {
            "avg": GRID_AVG,
            "min": GRID_MIN,
            "max": GRID_MAX,
            "center": GRID_CENTER,
        }.get(map_str, GRID_CENTER)
        self.ambient = cfg.get("ambient", 318.15)
        self.init_temp = cfg.get("init_temp", 333.15)

        self._build_layers(flp_units, layers_cfg)

        first_flp = self.layers[0].flp
        self.width = cfg.get("chip_width", _get_total_width(first_flp))
        self.height = cfg.get("chip_height", _get_total_height(first_flp))

        self._setup_bgmaps()

        self.pack = PackageRC()
        populate_package_R(self.pack, cfg, self.width, self.height)
        populate_package_C(self.pack, cfg, self.width, self.height)

        self._compute_layer_rc()

        self.total_n_blocks = sum(len(l.flp) for l in self.layers)
        extra = EXTRA + EXTRA_SEC if self.model_secondary else EXTRA
        self.n_nodes = self.nl * self.nr * self.nc + extra

        self.state = np.full(self.n_nodes, self.init_temp)

        self._build_sparse_matrix()
        self._build_capacitance_vector()

        n = self.n_nodes
        self._k1 = np.zeros(n)
        self._k2 = np.zeros(n)
        self._k3 = np.zeros(n)
        self._k4 = np.zeros(n)
        self._t_rk = np.zeros(n)
        self._ytemp = np.zeros(n)
        self._t1 = np.zeros(n)
        self._t2 = np.zeros(n)
        self._pow_vec = np.zeros(n)
        self._dv = np.zeros(n)

        self._euler_dt = None
        self._euler_A = None

    # Layer construction (populate_default_layers, append_package_layers)
    def _build_layers(self, flp_units, layers_cfg):
        """Build layer stack from config. Implements populate_default_layers() and
        append_package_layers() from temperature_grid.c.
        Each layer dict can carry 'flp' (list of FlpUnit) for per-layer floorplans.
        """
        cfg = self.cfg
        chip_layers = []

        if layers_cfg:
            for lc in layers_cfg:
                ly = Layer()
                ly.has_lateral = lc.get("has_lateral", True)
                ly.has_power = lc.get("has_power", False)
                ly.k = lc.get("k", 100.0)
                ly.thickness = lc.get("thickness", 0.15e-3)
                ly.sp = lc.get("sp", 1.75e6)
                ly.is_microchannel = lc.get("is_microchannel", False)
                per_flp = lc.get("flp", None)
                ly.flp = per_flp if per_flp is not None else flp_units
                if ly.is_microchannel and "mc_config" in lc:
                    mc = lc["mc_config"]
                    ly.mc_config = mc
                    _build_pressure_network(mc)
                    ly.flp = _build_mc_synthetic_flp(mc, self.nr, self.nc)
                    ly.has_lateral = True
                chip_layers.append(ly)
        else:
            si = Layer(
                has_lateral=True,
                has_power=True,
                k=cfg.get("k_chip", 100.0),
                thickness=cfg.get("t_chip", 0.15e-3),
                sp=cfg.get("p_chip", 1.75e6),
                flp=flp_units,
            )
            iface = Layer(
                has_lateral=True,
                has_power=False,
                k=cfg.get("k_interface", 4.0),
                thickness=cfg.get("t_interface", 20e-6),
                sp=cfg.get("p_interface", 4e6),
                flp=flp_units,
            )
            chip_layers = [si, iface]

        last_chip_flp = chip_layers[-1].flp
        sp_layer = Layer(
            has_lateral=True,
            has_power=False,
            k=cfg.get("k_spreader", 400.0),
            thickness=cfg.get("t_spreader", 1e-3),
            sp=cfg.get("p_spreader", 3.55e6),
            flp=last_chip_flp,
        )
        hs_layer = Layer(
            has_lateral=True,
            has_power=False,
            k=cfg.get("k_sink", 400.0),
            thickness=cfg.get("t_sink", 6.9e-3),
            sp=cfg.get("p_sink", 3.55e6),
            flp=last_chip_flp,
        )

        if self.model_secondary:
            first_chip_flp = chip_layers[0].flp
            sec_chip = [
                Layer(
                    has_lateral=True,
                    has_power=False,
                    k=K_C4,
                    thickness=cfg.get("t_c4", 0.0001),
                    sp=SPEC_HEAT_C4,
                    flp=first_chip_flp,
                ),
                Layer(
                    has_lateral=True,
                    has_power=False,
                    k=K_METAL,
                    thickness=cfg.get("t_metal", 10e-6),
                    sp=SPEC_HEAT_METAL,
                    flp=first_chip_flp,
                ),
            ]
            sec_pack = [
                Layer(
                    has_lateral=True,
                    has_power=False,
                    k=K_PCB,
                    thickness=cfg.get("t_pcb", 0.002),
                    sp=SPEC_HEAT_PCB,
                    flp=first_chip_flp,
                ),
                Layer(
                    has_lateral=True,
                    has_power=False,
                    k=K_SOLDER,
                    thickness=cfg.get("t_solder", 0.00094),
                    sp=SPEC_HEAT_SOLDER,
                    flp=first_chip_flp,
                ),
                Layer(
                    has_lateral=True,
                    has_power=False,
                    k=K_SUB,
                    thickness=cfg.get("t_sub", 0.001),
                    sp=SPEC_HEAT_SUB,
                    flp=first_chip_flp,
                ),
            ]
            all_layers = (
                sec_pack + sec_chip + chip_layers + [sp_layer, hs_layer]
            )
        else:
            all_layers = chip_layers + [sp_layer, hs_layer]

        for idx, ly in enumerate(all_layers):
            ly.no = idx
        self.layers = all_layers
        self.nl = len(all_layers)
        self.spidx = self.nl - DEFAULT_PACK_LAYERS + LAYER_SP
        self.hsidx = self.nl - DEFAULT_PACK_LAYERS + LAYER_SINK
        if self.model_secondary:
            self.subidx = LAYER_SUB_SEC
            self.solderidx = LAYER_SOLDER_SEC
            self.pcbidx = LAYER_PCB_SEC
        else:
            self.subidx = -1
            self.solderidx = -1
            self.pcbidx = -1

    # Block-grid mapping (set_bgmap from temperature_grid.c)
    def _setup_bgmaps(self):
        """Build b2gmap and g2bmap for each layer. Implements set_bgmap().
        Also populates per-cell R/C arrays for detailed_3D and microchannel layers.
        """
        cw = self.width / self.nc
        ch = self.height / self.nr
        done_flps = set()
        for layer in self.layers:
            flp_id = id(layer.flp)
            if (
                flp_id in done_flps
                and not self.detailed_3D
                and not layer.is_microchannel
            ):
                ref = None
                for prev in self.layers:
                    if id(prev.flp) == flp_id and prev.b2gmap is not None:
                        ref = prev
                        break
                if ref is not None:
                    layer.b2gmap = ref.b2gmap
                    layer.g2bmap = ref.g2bmap
                    continue
            done_flps.add(flp_id)
            b2g = [[[] for _ in range(self.nc)] for _ in range(self.nr)]
            g2b = []
            units = layer.flp
            for u_idx, unit in enumerate(units):
                lu = unit.leftx
                ru = lu + unit.width
                bu = unit.bottomy
                tu = bu + unit.height
                i1 = self.nr - tolerant_ceil(tu / ch)
                i2 = self.nr - tolerant_floor(bu / ch)
                j1 = tolerant_floor(lu / cw)
                j2 = tolerant_ceil(ru / cw)
                i1 = max(0, min(i1, self.nr))
                i2 = max(0, min(i2, self.nr))
                j1 = max(0, min(j1, self.nc))
                j2 = max(0, min(j2, self.nc))
                g2b.append((i1, i2, j1, j2))
                for i in range(i1, i2):
                    for j in range(j1, j2):
                        if i > i1 and i < i2 - 1 and j > j1 and j < j2 - 1:
                            b2g[i][j].append((u_idx, 1.0))
                        else:
                            lc_x = j * cw
                            rc_x = (j + 1) * cw
                            tc = self.height - i * ch
                            bc = self.height - (i + 1) * ch
                            oh = min(tu, tc) - max(bu, bc)
                            ow = min(ru, rc_x) - max(lu, lc_x)
                            if eq(oh / ch, 0):
                                oh = 0.0
                            elif eq(oh / ch, 1):
                                oh = ch
                            if eq(ow / cw, 0):
                                ow = 0.0
                            elif eq(ow / cw, 1):
                                ow = cw
                            occ = max(0.0, oh * ow) / (ch * cw)
                            if occ > 0:
                                b2g[i][j].append((u_idx, occ))
            layer.b2gmap = b2g
            layer.g2bmap = g2b

        if self.detailed_3D or any(ly.is_microchannel for ly in self.layers):
            self._populate_per_cell_rc(cw, ch)

    # Per-cell R/C for detailed_3D (find_res_3D / find_cap_3D from temperature_grid.c)
    def _populate_per_cell_rc(self, cw, ch):
        """Populate per-cell rx/ry/rz/cap arrays from per-block material overrides.
        Implements the per-cell portion of set_bgmap in temperature_grid.c."""
        nr, nc = self.nr, self.nc
        for layer in self.layers:
            crx = np.zeros((nr, nc))
            cry = np.zeros((nr, nc))
            crz = np.zeros((nr, nc))
            ccap = np.zeros((nr, nc))
            has_rc = np.zeros((nr, nc), dtype=bool)
            b2g = layer.b2gmap
            units = layer.flp
            any_custom = False

            for i in range(nr):
                for j in range(nc):
                    entries = b2g[i][j]
                    if (
                        len(entries) == 1
                        and entries[0][1] >= OCCUPANCY_THRESHOLD
                    ):
                        u = units[entries[0][0]]
                        if u.hasRes:
                            k_cell = (
                                1.0 / u.resistivity
                                if u.resistivity > 0
                                else layer.k
                            )
                            if layer.has_lateral:
                                crx[i, j] = getr(
                                    k_cell, cw, ch * layer.thickness
                                )
                                cry[i, j] = getr(
                                    k_cell, ch, cw * layer.thickness
                                )
                            else:
                                crx[i, j] = LARGENUM
                                cry[i, j] = LARGENUM
                            crz[i, j] = getr(k_cell, layer.thickness, cw * ch)
                            has_rc[i, j] = True
                            any_custom = True
                        if u.hasSh:
                            sp = (
                                u.specificheat
                                if u.specificheat > 0
                                else layer.sp
                            )
                            ccap[i, j] = getcap(sp, layer.thickness, cw * ch)
                            has_rc[i, j] = True
                            any_custom = True
                    elif len(entries) > 1:
                        g_rx = 0.0
                        g_ry = 0.0
                        g_rz = 0.0
                        sp_sum = 0.0
                        got_k = False
                        got_sp = False
                        for u_idx, occ in entries:
                            u = units[u_idx]
                            k_cell = (
                                1.0 / u.resistivity
                                if (u.hasRes and u.resistivity > 0)
                                else layer.k
                            )
                            if u.hasRes:
                                got_k = True
                            sp_cell = (
                                u.specificheat
                                if (u.hasSh and u.specificheat > 0)
                                else layer.sp
                            )
                            if u.hasSh:
                                got_sp = True
                            g_rx += occ / getr(
                                k_cell, cw, ch * layer.thickness
                            )
                            g_ry += occ / getr(
                                k_cell, ch, cw * layer.thickness
                            )
                            g_rz += occ / getr(
                                k_cell, layer.thickness, cw * ch
                            )
                            sp_sum += occ * sp_cell
                        if got_k:
                            if layer.has_lateral:
                                crx[i, j] = (
                                    1.0 / g_rx if g_rx > 0 else LARGENUM
                                )
                                cry[i, j] = (
                                    1.0 / g_ry if g_ry > 0 else LARGENUM
                                )
                            else:
                                crx[i, j] = LARGENUM
                                cry[i, j] = LARGENUM
                            crz[i, j] = 1.0 / g_rz if g_rz > 0 else LARGENUM
                            has_rc[i, j] = True
                            any_custom = True
                        if got_sp:
                            ccap[i, j] = getcap(
                                sp_sum, layer.thickness, cw * ch
                            )
                            has_rc[i, j] = True
                            any_custom = True

            if any_custom:
                layer.cell_rx = crx
                layer.cell_ry = cry
                layer.cell_rz = crz
                layer.cell_cap = ccap
                layer.has_custom_rc = has_rc

    # Layer R/C computation (populate_R/C_model_grid)
    def _compute_layer_rc(self):
        """Implements populate_R_model_grid() and populate_C_model_grid()."""
        cw = self.width / self.nc
        ch = self.height / self.nr
        cfg = self.cfg
        for i, ly in enumerate(self.layers):
            if ly.has_lateral:
                ly.rx = getr(ly.k, cw, ch * ly.thickness)
                ly.ry = getr(ly.k, ch, cw * ly.thickness)
            else:
                ly.rx = LARGENUM
                ly.ry = LARGENUM
            ly.rz = getr(ly.k, ly.thickness, cw * ch)
            if i == self.hsidx:
                ly.rz += (
                    cfg.get("r_convec", 0.1)
                    * (cfg.get("s_sink", 0.06) ** 2)
                    / (cw * ch)
                )
            ly.c = getcap(ly.sp, ly.thickness, cw * ch)

        hs = self.layers[self.hsidx]
        hs.c += (
            C_FACTOR
            * cfg.get("c_convec", 140.4)
            * (cw * ch)
            / (cfg.get("s_sink", 0.06) ** 2)
        )
        if self.model_secondary:
            pcb = self.layers[self.pcbidx]
            pcb.c += (
                C_FACTOR
                * cfg.get("c_convec_sec", 140.4)
                * (cw * ch)
                / (cfg.get("s_pcb", 0.1) ** 2)
            )

    # Per-cell resistance lookup (find_res / find_res_3D from temperature_grid.c)
    def _find_lateral_R(
        self, lx: Layer, i: int, j: int, direction: str
    ) -> float:
        """Return lateral thermal resistance for cell (i,j) in the given direction.
        direction is 'x' (N/S, row differs) or 'y' (W/E, col differs)."""
        if lx.has_custom_rc is not None and lx.has_custom_rc[i, j]:
            return lx.cell_rx[i, j] if direction == "x" else lx.cell_ry[i, j]
        return lx.rx if direction == "x" else lx.ry

    def _find_vertical_R(self, lx: Layer, i: int, j: int) -> float:
        """Return vertical thermal resistance for cell (i,j)."""
        if lx.has_custom_rc is not None and lx.has_custom_rc[i, j]:
            return lx.cell_rz[i, j]
        return lx.rz

    def _find_cap(self, lx: Layer, i: int, j: int) -> float:
        """Return per-cell capacitance for cell (i,j)."""
        if (
            lx.has_custom_rc is not None
            and lx.has_custom_rc[i, j]
            and lx.cell_cap[i, j] > 0
        ):
            return lx.cell_cap[i, j]
        return lx.c

    # Microchannel resistance helpers
    def _mc_lateral_R(self, mc, lx, i1, j1, i2, j2, R_base, cw, ch):
        """Modify lateral resistance for microchannel fluid-solid interface.
        Adds convective resistance 1/(htc*area) when one cell is fluid and the other solid.
        """
        ct = mc.cell_types
        f1 = _is_fluid_cell(ct, i1, j1)
        f2 = _is_fluid_cell(ct, i2, j2)
        if f1 == f2:
            return R_base
        if i1 != i2:
            face_area = cw * lx.thickness
        else:
            face_area = ch * lx.thickness
        r_conv = 1.0 / (mc.htc * face_area)
        return R_base + r_conv

    def _mc_vertical_R(self, lx_cur, lx_other, i, j, R_base, cw, ch):
        """Modify vertical resistance for microchannel layer boundary.
        Adds convective resistance when crossing fluid-solid boundary."""
        mc_cur = lx_cur.mc_config if lx_cur.is_microchannel else None
        mc_oth = lx_other.mc_config if lx_other.is_microchannel else None
        if mc_cur is not None and mc_oth is not None:
            return R_base
        if mc_cur is not None:
            ct = mc_cur.cell_types
            if _is_fluid_cell(ct, i, j):
                face_area = cw * ch
                r_conv = 1.0 / (mc_cur.htc * face_area)
                return R_base + r_conv
        if mc_oth is not None:
            ct = mc_oth.cell_types
            if _is_fluid_cell(ct, i, j):
                face_area = cw * ch
                r_conv = 1.0 / (mc_oth.htc * face_area)
                return R_base + r_conv
        return R_base

    # Sparse conductance matrix assembly (from build_transient_grid_matrix)
    def _build_sparse_matrix(self):
        """Build the conductance matrix G in COO format matching
        build_transient_grid_matrix() from temperature_grid.c.
        G is positive-definite: positive diagonal, negative off-diagonal."""
        nr, nc, nl = self.nr, self.nc, self.nl
        pk = self.pack
        lyr = self.layers
        spidx, hsidx = self.spidx, self.hsidx
        model_sec = self.model_secondary
        subidx, solderidx, pcbidx = self.subidx, self.solderidx, self.pcbidx
        cw = self.width / nc
        ch = self.height / nr
        use_3d = self.detailed_3D or any(ly.is_microchannel for ly in lyr)

        rows_list, cols_list, vals_list = [], [], []

        def _add(r, c, v):
            rows_list.append(r)
            cols_list.append(c)
            vals_list.append(v)

        for layer_idx in range(nl):
            lx = lyr[layer_idx]
            is_mc = lx.is_microchannel
            mc = lx.mc_config if is_mc else None
            for i in range(nr):
                for j in range(nc):
                    node = layer_idx * nr * nc + i * nc + j
                    dia = 0.0

                    if use_3d:
                        rx_cell = self._find_lateral_R(lx, i, j, "x")
                        ry_cell = self._find_lateral_R(lx, i, j, "y")
                        rz_cell = self._find_vertical_R(lx, i, j)
                    else:
                        rx_cell = lx.rx
                        ry_cell = lx.ry
                        rz_cell = lx.rz

                    if j > 0:
                        R2 = (
                            self._find_lateral_R(lx, i, j - 1, "y")
                            if use_3d
                            else lx.ry
                        )
                        R = (ry_cell + R2) / 2.0 if use_3d else ry_cell
                        if is_mc:
                            R = self._mc_lateral_R(
                                mc, lx, i, j, i, j - 1, R, cw, ch
                            )
                        _add(node, node - 1, -1.0 / R)
                        dia += 1.0 / R
                    if j < nc - 1:
                        R2 = (
                            self._find_lateral_R(lx, i, j + 1, "y")
                            if use_3d
                            else lx.ry
                        )
                        R = (ry_cell + R2) / 2.0 if use_3d else ry_cell
                        if is_mc:
                            R = self._mc_lateral_R(
                                mc, lx, i, j, i, j + 1, R, cw, ch
                            )
                        _add(node, node + 1, -1.0 / R)
                        dia += 1.0 / R
                    if i > 0:
                        R2 = (
                            self._find_lateral_R(lx, i - 1, j, "x")
                            if use_3d
                            else lx.rx
                        )
                        R = (rx_cell + R2) / 2.0 if use_3d else rx_cell
                        if is_mc:
                            R = self._mc_lateral_R(
                                mc, lx, i, j, i - 1, j, R, cw, ch
                            )
                        _add(node, node - nc, -1.0 / R)
                        dia += 1.0 / R
                    if i < nr - 1:
                        R2 = (
                            self._find_lateral_R(lx, i + 1, j, "x")
                            if use_3d
                            else lx.rx
                        )
                        R = (rx_cell + R2) / 2.0 if use_3d else rx_cell
                        if is_mc:
                            R = self._mc_lateral_R(
                                mc, lx, i, j, i + 1, j, R, cw, ch
                            )
                        _add(node, node + nc, -1.0 / R)
                        dia += 1.0 / R
                    # Above (layer_idx - 1)
                    if layer_idx > 0:
                        rz_above = (
                            self._find_vertical_R(lyr[layer_idx - 1], i, j)
                            if use_3d
                            else lyr[layer_idx - 1].rz
                        )
                        if use_3d:
                            if layer_idx - 1 == 0:
                                R = rz_above + rz_cell / 2.0
                            else:
                                R = rz_above / 2.0 + rz_cell / 2.0
                        else:
                            R = rz_above
                        if is_mc or lyr[layer_idx - 1].is_microchannel:
                            R = self._mc_vertical_R(
                                lx, lyr[layer_idx - 1], i, j, R, cw, ch
                            )
                        _add(node, node - nr * nc, -1.0 / R)
                        dia += 1.0 / R
                    # Below (layer_idx + 1)
                    if layer_idx < nl - 1:
                        rz_below = (
                            self._find_vertical_R(lyr[layer_idx + 1], i, j)
                            if use_3d
                            else lyr[layer_idx + 1].rz
                        )
                        if use_3d:
                            if layer_idx == 0:
                                R = rz_cell + rz_below / 2.0
                            else:
                                R = rz_cell / 2.0 + rz_below / 2.0
                        else:
                            R = rz_cell
                        if is_mc or lyr[layer_idx + 1].is_microchannel:
                            R = self._mc_vertical_R(
                                lx, lyr[layer_idx + 1], i, j, R, cw, ch
                            )
                        _add(node, node + nr * nc, -1.0 / R)
                        dia += 1.0 / R

                    # Microchannel advection terms (midpoint scheme from slope_fn_grid)
                    if is_mc and mc is not None:
                        ct = mc.cell_types
                        if _is_fluid_cell(ct, i, j):
                            cc_v = mc.coolant_capac
                            for di, dj in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                                ni, nj = i + di, j + dj
                                if (
                                    0 <= ni < nr
                                    and 0 <= nj < nc
                                    and _is_fluid_cell(ct, ni, nj)
                                ):
                                    Q = _mc_flow_rate(mc, ni, nj, i, j)
                                    nb_node = (
                                        layer_idx * nr * nc + ni * nc + nj
                                    )
                                    _add(node, nb_node, -(cc_v * Q / 2.0))
                                    dia -= cc_v * Q / 2.0
                        if ct[i, j] == MC_OUTLET:
                            out_sum = 0.0
                            for di, dj in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                                ni, nj = i + di, j + dj
                                if (
                                    0 <= ni < nr
                                    and 0 <= nj < nc
                                    and _is_fluid_cell(ct, ni, nj)
                                ):
                                    out_sum += _mc_flow_rate(mc, ni, nj, i, j)
                            dia += mc.coolant_capac * out_sum

                    pkg_base = nl * nr * nc

                    # Spreader boundary connections
                    if layer_idx == spidx:
                        if i == 0:
                            R = lx.ry / 2.0 + nc * pk.r_sp1_y
                            _add(node, pkg_base + SP_N, -1.0 / R)
                            dia += 1.0 / R
                        if i == nr - 1:
                            R = lx.ry / 2.0 + nc * pk.r_sp1_y
                            _add(node, pkg_base + SP_S, -1.0 / R)
                            dia += 1.0 / R
                        if j == nc - 1:
                            R = lx.rx / 2.0 + nr * pk.r_sp1_x
                            _add(node, pkg_base + SP_E, -1.0 / R)
                            dia += 1.0 / R
                        if j == 0:
                            R = lx.rx / 2.0 + nr * pk.r_sp1_x
                            _add(node, pkg_base + SP_W, -1.0 / R)
                            dia += 1.0 / R

                    # Heatsink boundary connections + ambient
                    elif layer_idx == hsidx:
                        dia += 1.0 / rz_cell
                        if i == 0:
                            R = lx.ry / 2.0 + nc * pk.r_hs1_y
                            _add(node, pkg_base + SINK_C_N, -1.0 / R)
                            dia += 1.0 / R
                        if i == nr - 1:
                            R = lx.ry / 2.0 + nc * pk.r_hs1_y
                            _add(node, pkg_base + SINK_C_S, -1.0 / R)
                            dia += 1.0 / R
                        if j == nc - 1:
                            R = lx.rx / 2.0 + nr * pk.r_hs1_x
                            _add(node, pkg_base + SINK_C_E, -1.0 / R)
                            dia += 1.0 / R
                        if j == 0:
                            R = lx.rx / 2.0 + nr * pk.r_hs1_x
                            _add(node, pkg_base + SINK_C_W, -1.0 / R)
                            dia += 1.0 / R

                    # Secondary path layer connections
                    elif model_sec and layer_idx == subidx:
                        if i == 0:
                            R = lx.ry / 2.0 + nc * pk.r_sub1_y
                            _add(node, pkg_base + SUB_N, -1.0 / R)
                            dia += 1.0 / R
                        if i == nr - 1:
                            R = lx.ry / 2.0 + nc * pk.r_sub1_y
                            _add(node, pkg_base + SUB_S, -1.0 / R)
                            dia += 1.0 / R
                        if j == nc - 1:
                            R = lx.rx / 2.0 + nr * pk.r_sub1_x
                            _add(node, pkg_base + SUB_E, -1.0 / R)
                            dia += 1.0 / R
                        if j == 0:
                            R = lx.rx / 2.0 + nr * pk.r_sub1_x
                            _add(node, pkg_base + SUB_W, -1.0 / R)
                            dia += 1.0 / R
                    elif model_sec and layer_idx == solderidx:
                        if i == 0:
                            R = lx.ry / 2.0 + nc * pk.r_solder1_y
                            _add(node, pkg_base + SOLDER_N, -1.0 / R)
                            dia += 1.0 / R
                        if i == nr - 1:
                            R = lx.ry / 2.0 + nc * pk.r_solder1_y
                            _add(node, pkg_base + SOLDER_S, -1.0 / R)
                            dia += 1.0 / R
                        if j == nc - 1:
                            R = lx.rx / 2.0 + nr * pk.r_solder1_x
                            _add(node, pkg_base + SOLDER_E, -1.0 / R)
                            dia += 1.0 / R
                        if j == 0:
                            R = lx.rx / 2.0 + nr * pk.r_solder1_x
                            _add(node, pkg_base + SOLDER_W, -1.0 / R)
                            dia += 1.0 / R
                    elif model_sec and layer_idx == pcbidx:
                        R_amb = (
                            self.cfg.get("r_convec_sec", 1.0)
                            * (self.cfg.get("s_pcb", 0.1) ** 2)
                            / (cw * ch)
                        )
                        dia += 1.0 / R_amb  # ambient
                        if i == 0:
                            R = lx.ry / 2.0 + nc * pk.r_pcb1_y
                            _add(node, pkg_base + PCB_C_N, -1.0 / R)
                            dia += 1.0 / R
                        if i == nr - 1:
                            R = lx.ry / 2.0 + nc * pk.r_pcb1_y
                            _add(node, pkg_base + PCB_C_S, -1.0 / R)
                            dia += 1.0 / R
                        if j == nc - 1:
                            R = lx.rx / 2.0 + nr * pk.r_pcb1_x
                            _add(node, pkg_base + PCB_C_E, -1.0 / R)
                            dia += 1.0 / R
                        if j == 0:
                            R = lx.rx / 2.0 + nr * pk.r_pcb1_x
                            _add(node, pkg_base + PCB_C_W, -1.0 / R)
                            dia += 1.0 / R

                    _add(node, node, dia)

        # Package node rows (from build_transient_grid_matrix)
        pkg = nl * nr * nc

        # SINK_N, SINK_S
        for sn, sc in [(SINK_N, SINK_C_N), (SINK_S, SINK_C_S)]:
            dia = 0.0
            R = pk.r_hs2_y + pk.r_hs
            _add(pkg + sn, pkg + sc, -1.0 / R)
            dia += 1.0 / R
            R = pk.r_hs_per + pk.r_amb_per
            dia += 1.0 / R
            _add(pkg + sn, pkg + sn, dia)
        # SINK_W, SINK_E
        for sn, sc in [(SINK_W, SINK_C_W), (SINK_E, SINK_C_E)]:
            dia = 0.0
            R = pk.r_hs2_x + pk.r_hs
            _add(pkg + sn, pkg + sc, -1.0 / R)
            dia += 1.0 / R
            R = pk.r_hs_per + pk.r_amb_per
            dia += 1.0 / R
            _add(pkg + sn, pkg + sn, dia)

        # SINK_C_N, SINK_C_S — connected to heatsink boundary row + SP + SINK outer + ambient
        for sc_node, sp_node, sink_outer, boundary_row in [
            (SINK_C_N, SP_N, SINK_N, 0),
            (SINK_C_S, SP_S, SINK_S, nr - 1),
        ]:
            dia = 0.0
            R_hs_edge = lyr[hsidx].ry / 2.0 + nc * pk.r_hs1_y
            for jj in range(nc):
                grid_node = hsidx * nr * nc + boundary_row * nc + jj
                _add(pkg + sc_node, grid_node, -1.0 / R_hs_edge)
                dia += 1.0 / R_hs_edge
            R = pk.r_sp_per_y
            _add(pkg + sc_node, pkg + sp_node, -1.0 / R)
            dia += 1.0 / R
            R = pk.r_hs2_y + pk.r_hs
            _add(pkg + sc_node, pkg + sink_outer, -1.0 / R)
            dia += 1.0 / R
            R = pk.r_hs_c_per_y + pk.r_amb_c_per_y
            dia += 1.0 / R
            _add(pkg + sc_node, pkg + sc_node, dia)

        # SINK_C_W, SINK_C_E
        for sc_node, sp_node, sink_outer, boundary_col in [
            (SINK_C_W, SP_W, SINK_W, 0),
            (SINK_C_E, SP_E, SINK_E, nc - 1),
        ]:
            dia = 0.0
            R_hs_edge = lyr[hsidx].rx / 2.0 + nr * pk.r_hs1_x
            for ii in range(nr):
                grid_node = hsidx * nr * nc + ii * nc + boundary_col
                _add(pkg + sc_node, grid_node, -1.0 / R_hs_edge)
                dia += 1.0 / R_hs_edge
            R = pk.r_sp_per_x
            _add(pkg + sc_node, pkg + sp_node, -1.0 / R)
            dia += 1.0 / R
            R = pk.r_hs2_x + pk.r_hs
            _add(pkg + sc_node, pkg + sink_outer, -1.0 / R)
            dia += 1.0 / R
            R = pk.r_hs_c_per_x + pk.r_amb_c_per_x
            dia += 1.0 / R
            _add(pkg + sc_node, pkg + sc_node, dia)

        # SP_N, SP_S — connected to spreader boundary row + SINK_C
        for sp_node, sc_node, boundary_row in [
            (SP_N, SINK_C_N, 0),
            (SP_S, SINK_C_S, nr - 1),
        ]:
            dia = 0.0
            R_sp_edge = lyr[spidx].ry / 2.0 + nc * pk.r_sp1_y
            for jj in range(nc):
                grid_node = spidx * nr * nc + boundary_row * nc + jj
                _add(pkg + sp_node, grid_node, -1.0 / R_sp_edge)
                dia += 1.0 / R_sp_edge
            R = pk.r_sp_per_y
            _add(pkg + sp_node, pkg + sc_node, -1.0 / R)
            dia += 1.0 / R
            _add(pkg + sp_node, pkg + sp_node, dia)

        # SP_W, SP_E
        for sp_node, sc_node, boundary_col in [
            (SP_W, SINK_C_W, 0),
            (SP_E, SINK_C_E, nc - 1),
        ]:
            dia = 0.0
            R_sp_edge = lyr[spidx].rx / 2.0 + nr * pk.r_sp1_x
            for ii in range(nr):
                grid_node = spidx * nr * nc + ii * nc + boundary_col
                _add(pkg + sp_node, grid_node, -1.0 / R_sp_edge)
                dia += 1.0 / R_sp_edge
            R = pk.r_sp_per_x
            _add(pkg + sp_node, pkg + sc_node, -1.0 / R)
            dia += 1.0 / R
            _add(pkg + sp_node, pkg + sp_node, dia)

        # Secondary path package nodes
        if model_sec:
            self._build_secondary_pkg_nodes(
                rows_list, cols_list, vals_list, _add
            )

        n = self.n_nodes
        self._G = sparse.coo_matrix(
            (vals_list, (rows_list, cols_list)), shape=(n, n)
        ).tocsc()

    def _build_secondary_pkg_nodes(self, rows_l, cols_l, vals_l, _add):
        nr, nc, nl = self.nr, self.nc, self.nl
        pk = self.pack
        lyr = self.layers
        pkg = nl * nr * nc
        pcbidx = self.pcbidx
        solderidx = self.solderidx
        subidx = self.subidx

        # PCB_N/S
        for pn, pcn in [(PCB_N, PCB_C_N), (PCB_S, PCB_C_S)]:
            dia = 0.0
            R = pk.r_pcb2_y + pk.r_pcb
            _add(pkg + pn, pkg + pcn, -1.0 / R)
            dia += 1.0 / R
            R = pk.r_amb_sec_per
            dia += 1.0 / R
            _add(pkg + pn, pkg + pn, dia)
        for pn, pcn in [(PCB_W, PCB_C_W), (PCB_E, PCB_C_E)]:
            dia = 0.0
            R = pk.r_pcb2_x + pk.r_pcb
            _add(pkg + pn, pkg + pcn, -1.0 / R)
            dia += 1.0 / R
            R = pk.r_amb_sec_per
            dia += 1.0 / R
            _add(pkg + pn, pkg + pn, dia)

        # PCB_C_N/S
        for pcn, soln, pcb_outer, brow in [
            (PCB_C_N, SOLDER_N, PCB_N, 0),
            (PCB_C_S, SOLDER_S, PCB_S, nr - 1),
        ]:
            dia = 0.0
            R_edge = lyr[pcbidx].ry / 2.0 + nc * pk.r_pcb1_y
            for jj in range(nc):
                gn = pcbidx * nr * nc + brow * nc + jj
                _add(pkg + pcn, gn, -1.0 / R_edge)
                dia += 1.0 / R_edge
            R = pk.r_pcb_c_per_y
            _add(pkg + pcn, pkg + soln, -1.0 / R)
            dia += 1.0 / R
            R = pk.r_pcb2_y + pk.r_pcb
            _add(pkg + pcn, pkg + pcb_outer, -1.0 / R)
            dia += 1.0 / R
            R = pk.r_amb_sec_c_per_y
            dia += 1.0 / R
            _add(pkg + pcn, pkg + pcn, dia)
        for pcn, soln, pcb_outer, bcol in [
            (PCB_C_W, SOLDER_W, PCB_W, 0),
            (PCB_C_E, SOLDER_E, PCB_E, nc - 1),
        ]:
            dia = 0.0
            R_edge = lyr[pcbidx].rx / 2.0 + nr * pk.r_pcb1_x
            for ii in range(nr):
                gn = pcbidx * nr * nc + ii * nc + bcol
                _add(pkg + pcn, gn, -1.0 / R_edge)
                dia += 1.0 / R_edge
            R = pk.r_pcb_c_per_x
            _add(pkg + pcn, pkg + soln, -1.0 / R)
            dia += 1.0 / R
            R = pk.r_pcb2_x + pk.r_pcb
            _add(pkg + pcn, pkg + pcb_outer, -1.0 / R)
            dia += 1.0 / R
            R = pk.r_amb_sec_c_per_x
            dia += 1.0 / R
            _add(pkg + pcn, pkg + pcn, dia)

        # SOLDER_N/S
        for soln, subn, pcbn, brow in [
            (SOLDER_N, SUB_N, PCB_C_N, 0),
            (SOLDER_S, SUB_S, PCB_C_S, nr - 1),
        ]:
            dia = 0.0
            R_edge = lyr[solderidx].ry / 2.0 + nc * pk.r_solder1_y
            for jj in range(nc):
                gn = solderidx * nr * nc + brow * nc + jj
                _add(pkg + soln, gn, -1.0 / R_edge)
                dia += 1.0 / R_edge
            R = pk.r_solder_per_y
            _add(pkg + soln, pkg + subn, -1.0 / R)
            dia += 1.0 / R
            R = pk.r_pcb_c_per_y
            _add(pkg + soln, pkg + pcbn, -1.0 / R)
            dia += 1.0 / R
            _add(pkg + soln, pkg + soln, dia)
        for soln, subn, pcbn, bcol in [
            (SOLDER_W, SUB_W, PCB_C_W, 0),
            (SOLDER_E, SUB_E, PCB_C_E, nc - 1),
        ]:
            dia = 0.0
            R_edge = lyr[solderidx].rx / 2.0 + nr * pk.r_solder1_x
            for ii in range(nr):
                gn = solderidx * nr * nc + ii * nc + bcol
                _add(pkg + soln, gn, -1.0 / R_edge)
                dia += 1.0 / R_edge
            R = pk.r_solder_per_x
            _add(pkg + soln, pkg + subn, -1.0 / R)
            dia += 1.0 / R
            R = pk.r_pcb_c_per_x
            _add(pkg + soln, pkg + pcbn, -1.0 / R)
            dia += 1.0 / R
            _add(pkg + soln, pkg + soln, dia)

        # SUB_N/S
        for subn, soln, brow in [
            (SUB_N, SOLDER_N, 0),
            (SUB_S, SOLDER_S, nr - 1),
        ]:
            dia = 0.0
            R_edge = lyr[subidx].ry / 2.0 + nc * pk.r_sub1_y
            for jj in range(nc):
                gn = subidx * nr * nc + brow * nc + jj
                _add(pkg + subn, gn, -1.0 / R_edge)
                dia += 1.0 / R_edge
            R = pk.r_solder_per_y
            _add(pkg + subn, pkg + soln, -1.0 / R)
            dia += 1.0 / R
            _add(pkg + subn, pkg + subn, dia)
        for subn, soln, bcol in [
            (SUB_W, SOLDER_W, 0),
            (SUB_E, SOLDER_E, nc - 1),
        ]:
            dia = 0.0
            R_edge = lyr[subidx].rx / 2.0 + nr * pk.r_sub1_x
            for ii in range(nr):
                gn = subidx * nr * nc + ii * nc + bcol
                _add(pkg + subn, gn, -1.0 / R_edge)
                dia += 1.0 / R_edge
            R = pk.r_solder_per_x
            _add(pkg + subn, pkg + soln, -1.0 / R)
            dia += 1.0 / R
            _add(pkg + subn, pkg + subn, dia)

    # Capacitance vector (from build_diagonal_matrix)
    def _build_capacitance_vector(self):
        """Build the diagonal capacitance vector matching build_diagonal_matrix()."""
        nr, nc, nl = self.nr, self.nc, self.nl
        pk = self.pack
        C = np.zeros(self.n_nodes)
        use_3d = self.detailed_3D or any(
            ly.is_microchannel for ly in self.layers
        )
        for layer_idx in range(nl):
            lx = self.layers[layer_idx]
            off = layer_idx * nr * nc
            if use_3d and lx.has_custom_rc is not None:
                for i in range(nr):
                    for j in range(nc):
                        C[off + i * nc + j] = self._find_cap(lx, i, j)
            else:
                C[off : off + nr * nc] = lx.c
        base = nl * nr * nc
        C[base + SP_W] = pk.c_sp_per_x
        C[base + SP_E] = pk.c_sp_per_x
        C[base + SP_N] = pk.c_sp_per_y
        C[base + SP_S] = pk.c_sp_per_y
        C[base + SINK_C_W] = pk.c_hs_c_per_x + pk.c_amb_c_per_x
        C[base + SINK_C_E] = pk.c_hs_c_per_x + pk.c_amb_c_per_x
        C[base + SINK_C_N] = pk.c_hs_c_per_y + pk.c_amb_c_per_y
        C[base + SINK_C_S] = pk.c_hs_c_per_y + pk.c_amb_c_per_y
        C[base + SINK_W] = pk.c_hs_per + pk.c_amb_per
        C[base + SINK_E] = pk.c_hs_per + pk.c_amb_per
        C[base + SINK_N] = pk.c_hs_per + pk.c_amb_per
        C[base + SINK_S] = pk.c_hs_per + pk.c_amb_per
        if self.model_secondary:
            C[base + SUB_W] = pk.c_sub_per_x
            C[base + SUB_E] = pk.c_sub_per_x
            C[base + SUB_N] = pk.c_sub_per_y
            C[base + SUB_S] = pk.c_sub_per_y
            C[base + SOLDER_W] = pk.c_solder_per_x
            C[base + SOLDER_E] = pk.c_solder_per_x
            C[base + SOLDER_N] = pk.c_solder_per_y
            C[base + SOLDER_S] = pk.c_solder_per_y
            C[base + PCB_C_W] = pk.c_pcb_c_per_x + pk.c_amb_sec_c_per_x
            C[base + PCB_C_E] = pk.c_pcb_c_per_x + pk.c_amb_sec_c_per_x
            C[base + PCB_C_N] = pk.c_pcb_c_per_y + pk.c_amb_sec_c_per_y
            C[base + PCB_C_S] = pk.c_pcb_c_per_y + pk.c_amb_sec_c_per_y
            C[base + PCB_W] = pk.c_pcb_per + pk.c_amb_sec_per
            C[base + PCB_E] = pk.c_pcb_per + pk.c_amb_sec_per
            C[base + PCB_N] = pk.c_pcb_per + pk.c_amb_sec_per
            C[base + PCB_S] = pk.c_pcb_per + pk.c_amb_sec_per
        self._C_vec = C
        self._inv_C = np.where(C > 0, 1.0 / C, 0.0)

    # Power vector (from build_transient_power_vector)
    def _build_power_vector(self, power_grid: np.ndarray) -> np.ndarray:
        """Build the RHS power vector. power_grid is 3D [nl, nr, nc] of power per cell."""
        nr, nc, nl = self.nr, self.nc, self.nl
        P = self._pow_vec
        P[:] = 0.0
        cw = self.width / nc
        ch = self.height / nr
        for layer_idx in range(nl):
            off = layer_idx * nr * nc
            lx = self.layers[layer_idx]
            pg = power_grid[layer_idx]
            P[off : off + nr * nc] = pg.ravel()
            if layer_idx == self.hsidx:
                for i in range(nr):
                    for j in range(nc):
                        rz_val = (
                            self._find_vertical_R(lx, i, j)
                            if (
                                self.detailed_3D
                                and lx.has_custom_rc is not None
                            )
                            else lx.rz
                        )
                        P[off + i * nc + j] += self.ambient / rz_val
            elif self.model_secondary and layer_idx == self.pcbidx:
                R_amb = (
                    self.cfg.get("r_convec_sec", 1.0)
                    * (self.cfg.get("s_pcb", 0.1) ** 2)
                    / (cw * ch)
                )
                P[off : off + nr * nc] += self.ambient / R_amb
            if lx.is_microchannel and lx.mc_config is not None:
                mc = lx.mc_config
                ct = mc.cell_types
                cc_v = mc.coolant_capac
                for i in range(nr):
                    for j in range(nc):
                        if ct[i, j] == MC_INLET:
                            inlet_flow_sum = 0.0
                            for di, dj in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                                ni, nj = i + di, j + dj
                                if (
                                    0 <= ni < nr
                                    and 0 <= nj < nc
                                    and _is_fluid_cell(ct, ni, nj)
                                ):
                                    inlet_flow_sum += _mc_flow_rate(
                                        mc, ni, nj, i, j
                                    )
                            inlet_flow_rate = -inlet_flow_sum
                            P[off + i * nc + j] += (
                                cc_v * inlet_flow_rate * mc.inlet_temperature
                            )
        # Package nodes: ambient source terms
        base = nl * nr * nc
        pk = self.pack
        P[base + SINK_N] = self.ambient / (pk.r_hs_per + pk.r_amb_per)
        P[base + SINK_S] = self.ambient / (pk.r_hs_per + pk.r_amb_per)
        P[base + SINK_W] = self.ambient / (pk.r_hs_per + pk.r_amb_per)
        P[base + SINK_E] = self.ambient / (pk.r_hs_per + pk.r_amb_per)
        P[base + SINK_C_N] = self.ambient / (
            pk.r_hs_c_per_y + pk.r_amb_c_per_y
        )
        P[base + SINK_C_S] = self.ambient / (
            pk.r_hs_c_per_y + pk.r_amb_c_per_y
        )
        P[base + SINK_C_W] = self.ambient / (
            pk.r_hs_c_per_x + pk.r_amb_c_per_x
        )
        P[base + SINK_C_E] = self.ambient / (
            pk.r_hs_c_per_x + pk.r_amb_c_per_x
        )
        if self.model_secondary:
            P[base + PCB_N] = self.ambient / pk.r_amb_sec_per
            P[base + PCB_S] = self.ambient / pk.r_amb_sec_per
            P[base + PCB_W] = self.ambient / pk.r_amb_sec_per
            P[base + PCB_E] = self.ambient / pk.r_amb_sec_per
            P[base + PCB_C_N] = self.ambient / pk.r_amb_sec_c_per_y
            P[base + PCB_C_S] = self.ambient / pk.r_amb_sec_c_per_y
            P[base + PCB_C_W] = self.ambient / pk.r_amb_sec_c_per_x
            P[base + PCB_C_E] = self.ambient / pk.r_amb_sec_c_per_x
        return P

    # Power distribution (xlate_vector_b2g)
    def _distribute_power(self, power_W: dict[str, float]) -> np.ndarray:
        """Distribute block-level power to grid cells. Implements xlate_vector_b2g()."""
        nr, nc, nl = self.nr, self.nc, self.nl
        cell_area = (self.width * self.height) / (nc * nr)
        power_grid = np.zeros((nl, nr, nc))
        for layer_idx in range(nl):
            ly = self.layers[layer_idx]
            if not ly.has_power and not ly.is_microchannel:
                continue
            if ly.has_power:
                block_power = np.zeros(len(ly.flp))
                for bi, u in enumerate(ly.flp):
                    block_power[bi] = power_W.get(u.name, 0.0)
                b2g = ly.b2gmap
                for i in range(nr):
                    for j in range(nc):
                        val = 0.0
                        for bidx, occ in b2g[i][j]:
                            u = ly.flp[bidx]
                            val += (
                                occ * block_power[bidx] / (u.width * u.height)
                            )
                        power_grid[layer_idx, i, j] = val * cell_area
        return power_grid

    # Temperature mapping grid->block (xlate_temp_g2b)
    def _grid_to_block_temps(self) -> dict[str, float]:
        """Map grid temperatures to block temperatures. Implements xlate_temp_g2b()."""
        nr, nc, nl = self.nr, self.nc, self.nl
        v = self.state
        result = {}
        for layer_idx in range(nl):
            ly = self.layers[layer_idx]
            if not ly.has_power:
                continue
            g2b = ly.g2bmap
            for u_idx, unit in enumerate(ly.flp):
                i1, i2, j1, j2 = g2b[u_idx]
                off = layer_idx * nr * nc
                if self.map_mode == GRID_CENTER:
                    ci1 = (i1 + i2) // 2
                    cj1 = (j1 + j2) // 2
                    ci2 = ci1 - (0 if (i2 - i1) % 2 else 1)
                    cj2 = cj1 - (0 if (j2 - j1) % 2 else 1)
                    ci2 = max(ci2, i1)
                    cj2 = max(cj2, j1)
                    t = (
                        v[off + ci1 * nc + cj1]
                        + v[off + ci2 * nc + cj1]
                        + v[off + ci1 * nc + cj2]
                        + v[off + ci2 * nc + cj2]
                    ) / 4.0
                elif self.map_mode == GRID_AVG:
                    s = 0.0
                    cnt = 0
                    for ii in range(i1, i2):
                        for jj in range(j1, j2):
                            s += v[off + ii * nc + jj]
                            cnt += 1
                    t = s / cnt if cnt > 0 else self.ambient
                elif self.map_mode == GRID_MIN:
                    t = min(
                        v[off + ii * nc + jj]
                        for ii in range(i1, i2)
                        for jj in range(j1, j2)
                    )
                else:  # GRID_MAX
                    t = max(
                        v[off + ii * nc + jj]
                        for ii in range(i1, i2)
                        for jj in range(j1, j2)
                    )
                result[unit.name] = t
        return result

    # Slope function for RK4 (slope_fn_grid + slope_fn_pack)
    def _slope(self, v: np.ndarray, P: np.ndarray, dv: np.ndarray):
        """Compute dv = (P - G*v) / C. This matches slope_fn_grid() + slope_fn_pack()."""
        np.copyto(dv, P)
        dv -= self._G.dot(v)
        dv *= self._inv_C

    # RK4 integrator (rk4_core + rk4 from RCutil.c)
    def _rk4_core(self, y, k1, P, n, h, yout):
        """Standard 4th-order Runge-Kutta step. Implements rk4_core() from RCutil.c."""
        t = self._t_rk
        k2 = self._k2
        k3 = self._k3
        k4 = self._k4

        np.add(y, (h / 2.0) * k1, out=t)
        self._slope(t, P, k2)

        np.add(y, (h / 2.0) * k2, out=t)
        self._slope(t, P, k3)

        np.add(y, h * k3, out=t)
        self._slope(t, P, k4)

        yout[:] = y + (h / 6.0) * (k1 + 2.0 * k2 + 2.0 * k3 + k4)

    def _rk4_adaptive(self, y, P, h_in):
        """Adaptive RK4 with step-size control. Implements rk4() from RCutil.c.
        Returns (h_used, new_h) where h_used is the accepted step and
        new_h is the suggested step for next call."""
        n = len(y)
        k1 = self._k1
        ytemp = self._ytemp
        t1 = self._t1
        t2 = self._t2

        self._slope(y, P, k1)

        new_h = h_in
        while True:
            h = new_h
            self._rk4_core(y, k1, P, n, h, ytemp)
            self._rk4_core(y, k1, P, n, h / 2.0, t1)
            # Overwrite k1 with slope at midpoint — matches C code behavior
            self._slope(t1, P, k1)
            self._rk4_core(t1, k1, P, n, h / 2.0, t2)

            mx = np.max(np.abs(ytemp - t2))
            if mx == 0.0:
                mx = MIN_STEP

            if mx <= RK4_PRECISION:
                new_h = RK4_SAFETY * h * (abs(RK4_PRECISION / mx) ** 0.2)
                if new_h > RK4_MAXUP * h:
                    new_h = RK4_MAXUP * h
            else:
                new_h = RK4_SAFETY * h * (abs(RK4_PRECISION / mx) ** 0.25)
                if new_h < h / RK4_MAXDOWN:
                    new_h = h / RK4_MAXDOWN

            if new_h >= h:
                break
            # On retry, k1 is NOT recomputed at y — matches C code

        y[:] = ytemp
        return h, new_h

    # Steady-state solver (using spsolve)
    def steady_state(self, power_W: dict[str, float]) -> dict[str, float]:
        """Solve G*T = P for steady-state temperatures. Implements
        steady_state_temp_grid() / direct_SLU()."""
        power_grid = self._distribute_power(power_W)
        P = self._build_power_vector(power_grid)
        T = spsolve(self._G, P)
        self.state[:] = T
        return self._grid_to_block_temps()

    # Backward-Euler transient
    def step_euler(
        self, power_W: dict[str, float], dt: float
    ) -> dict[str, float]:
        """One backward-Euler step: (C/dt + G)*T_new = C/dt*T_old + P.
        Implements backward_euler() from RCutil.c."""
        power_grid = self._distribute_power(power_W)
        P = self._build_power_vector(power_grid)

        if self._euler_dt != dt:
            C_over_dt = sparse.diags(self._C_vec / dt)
            self._euler_A = (C_over_dt + self._G).tocsc()
            self._euler_dt = dt

        rhs = (self._C_vec / dt) * self.state + P
        self.state[:] = spsolve(self._euler_A, rhs)
        return self._grid_to_block_temps()

    # RK4 transient
    def step_rk4(
        self, power_W: dict[str, float], dt: float
    ) -> dict[str, float]:
        """Advance state by dt using adaptive RK4. Implements compute_temp_grid()
        with the same outer time loop as temperature_grid.c."""
        power_grid = self._distribute_power(power_W)
        P = self._build_power_vector(power_grid)

        t = 0.0
        new_h = MIN_STEP
        while t < dt and new_h >= MIN_STEP * DELTA:
            h = new_h
            h_used, new_h = self._rk4_adaptive(self.state, P, h)
            new_h = min(new_h, dt - t - h_used)
            t += h_used
        return self._grid_to_block_temps()


# ---------------------------------------------------------------------------
# Block Model
# ---------------------------------------------------------------------------
class _BlockModel:
    """Internal block model. Implements block_model_t from temperature_block.c."""

    def __init__(self, flp_units: list[FlpUnit], cfg: dict):
        self.cfg = cfg
        self.flp = flp_units
        self.n_units = len(flp_units)
        n = self.n_units
        self.n_nodes = NL * n + EXTRA
        self.ambient = cfg.get("ambient", 318.15)
        self.init_temp = cfg.get("init_temp", 333.15)

        self.w_chip = _get_total_width(flp_units)
        self.l_chip = _get_total_height(flp_units)

        self.pack = PackageRC()
        populate_package_R(self.pack, cfg, self.w_chip, self.l_chip)
        populate_package_C(self.pack, cfg, self.w_chip, self.l_chip)

        self._build_R_model()
        self._build_C_model()

        self.state = np.full(self.n_nodes, self.init_temp)

        nn = self.n_nodes
        self._k1 = np.zeros(nn)
        self._k2 = np.zeros(nn)
        self._k3 = np.zeros(nn)
        self._k4 = np.zeros(nn)
        self._t_rk = np.zeros(nn)
        self._ytemp = np.zeros(nn)
        self._t1 = np.zeros(nn)
        self._t2 = np.zeros(nn)
        self._t_vector = np.zeros(nn)
        self._dv = np.zeros(nn)

        self._euler_dt = None
        self._euler_A = None

    def _build_R_model(self):
        """Build conductance matrix B. Implements populate_R_model_block()."""
        flp = self.flp
        n = self.n_units
        m = self.n_nodes
        cfg = self.cfg
        t_chip = cfg.get("t_chip", 0.15e-3)
        k_chip = cfg.get("k_chip", 100.0)
        k_interface = cfg.get("k_interface", 4.0)
        k_spreader = cfg.get("k_spreader", 400.0)
        k_sink = cfg.get("k_sink", 400.0)
        t_interface = cfg.get("t_interface", 20e-6)
        t_spreader = cfg.get("t_spreader", 1e-3)
        t_sink = cfg.get("t_sink", 6.9e-3)
        r_convec = cfg.get("r_convec", 0.1)
        s_sink = cfg.get("s_sink", 0.06)
        block_omit = cfg.get("block_omit_lateral", False)

        gx = np.zeros(n)
        gy = np.zeros(n)
        gx_int = np.zeros(n)
        gy_int = np.zeros(n)
        gx_sp = np.zeros(n)
        gy_sp = np.zeros(n)
        gx_hs = np.zeros(n)
        gy_hs = np.zeros(n)
        g_amb = np.zeros(n + EXTRA)

        for i in range(n):
            w = flp[i].width
            h = flp[i].height
            if block_omit:
                gx[i] = gy[i] = 0.0
            else:
                gx[i] = 1.0 / getr(k_chip, w / 2.0, h * t_chip)
                gy[i] = 1.0 / getr(k_chip, h / 2.0, w * t_chip)
            gx_int[i] = 1.0 / getr(k_interface, w / 2.0, h * t_interface)
            gy_int[i] = 1.0 / getr(k_interface, h / 2.0, w * t_interface)
            gx_sp[i] = 1.0 / getr(k_spreader, w / 2.0, h * t_spreader)
            gy_sp[i] = 1.0 / getr(k_spreader, h / 2.0, w * t_spreader)
            gx_hs[i] = 1.0 / getr(k_sink, w / 2.0, h * t_sink)
            gy_hs[i] = 1.0 / getr(k_sink, h / 2.0, w * t_sink)

        length = np.zeros((n, n))
        for i in range(n):
            for j in range(i, n):
                length[i, j] = length[j, i] = _get_shared_len(flp, i, j)

        border = np.zeros((n, 4), dtype=int)  # W=0, E=1, N=2, S=3
        gn_sp = gs_sp = ge_sp = gw_sp = 0.0
        gn_hs = gs_hs = ge_hs = gw_hs = 0.0
        for i in range(n):
            if eq(flp[i].bottomy + flp[i].height, self.l_chip):
                gn_sp += gy_sp[i]
                gn_hs += gy_hs[i]
                border[i, 2] = 1
            if eq(flp[i].bottomy, 0):
                gs_sp += gy_sp[i]
                gs_hs += gy_hs[i]
                border[i, 3] = 1
            if eq(flp[i].leftx + flp[i].width, self.w_chip):
                ge_sp += gx_sp[i]
                ge_hs += gx_hs[i]
                border[i, 1] = 1
            if eq(flp[i].leftx, 0):
                gw_sp += gx_sp[i]
                gw_hs += gx_hs[i]
                border[i, 0] = 1

        g = np.zeros((m, m))
        for i in range(n):
            area = flp[i].height * flp[i].width
            for j in range(n):
                part = part_int = part_sp = part_hs = 0.0
                if _is_horiz_adj(flp, i, j):
                    part = gx[i] / flp[i].height
                    part_int = gx_int[i] / flp[i].height
                    part_sp = gx_sp[i] / flp[i].height
                    part_hs = gx_hs[i] / flp[i].height
                elif _is_vert_adj(flp, i, j):
                    part = gy[i] / flp[i].width
                    part_int = gy_int[i] / flp[i].width
                    part_sp = gy_sp[i] / flp[i].width
                    part_hs = gy_hs[i] / flp[i].width
                g[i][j] = part * length[i][j]
                g[IFACE * n + i][IFACE * n + j] = part_int * length[i][j]
                g[HSP * n + i][HSP * n + j] = part_sp * length[i][j]
                g[HSINK * n + i][HSINK * n + j] = part_hs * length[i][j]

            g[i][IFACE * n + i] = g[IFACE * n + i][i] = 2.0 / getr(
                k_chip, t_chip, area
            )
            g[IFACE * n + i][HSP * n + i] = g[HSP * n + i][IFACE * n + i] = (
                2.0 / getr(k_interface, t_interface, area)
            )
            g[HSP * n + i][HSINK * n + i] = g[HSINK * n + i][HSP * n + i] = (
                2.0 / getr(k_spreader, t_spreader, area)
            )
            r_amb = r_convec * (s_sink * s_sink) / area
            g_amb[i] = 1.0 / (getr(k_sink, t_sink, area) + r_amb)

            pk = self.pack
            g[HSP * n + i][NL * n + SP_N] = g[NL * n + SP_N][HSP * n + i] = (
                2.0
                * border[i, 2]
                / ((1.0 / gy_sp[i]) + pk.r_sp1_y * gn_sp / gy_sp[i])
                if gy_sp[i] != 0
                else 0.0
            )
            g[HSP * n + i][NL * n + SP_S] = g[NL * n + SP_S][HSP * n + i] = (
                2.0
                * border[i, 3]
                / ((1.0 / gy_sp[i]) + pk.r_sp1_y * gs_sp / gy_sp[i])
                if gy_sp[i] != 0
                else 0.0
            )
            g[HSP * n + i][NL * n + SP_E] = g[NL * n + SP_E][HSP * n + i] = (
                2.0
                * border[i, 1]
                / ((1.0 / gx_sp[i]) + pk.r_sp1_x * ge_sp / gx_sp[i])
                if gx_sp[i] != 0
                else 0.0
            )
            g[HSP * n + i][NL * n + SP_W] = g[NL * n + SP_W][HSP * n + i] = (
                2.0
                * border[i, 0]
                / ((1.0 / gx_sp[i]) + pk.r_sp1_x * gw_sp / gx_sp[i])
                if gx_sp[i] != 0
                else 0.0
            )

            g[HSINK * n + i][NL * n + SINK_C_N] = g[NL * n + SINK_C_N][
                HSINK * n + i
            ] = (
                2.0
                * border[i, 2]
                / ((1.0 / gy_hs[i]) + pk.r_hs1_y * gn_hs / gy_hs[i])
                if gy_hs[i] != 0
                else 0.0
            )
            g[HSINK * n + i][NL * n + SINK_C_S] = g[NL * n + SINK_C_S][
                HSINK * n + i
            ] = (
                2.0
                * border[i, 3]
                / ((1.0 / gy_hs[i]) + pk.r_hs1_y * gs_hs / gy_hs[i])
                if gy_hs[i] != 0
                else 0.0
            )
            g[HSINK * n + i][NL * n + SINK_C_E] = g[NL * n + SINK_C_E][
                HSINK * n + i
            ] = (
                2.0
                * border[i, 1]
                / ((1.0 / gx_hs[i]) + pk.r_hs1_x * ge_hs / gx_hs[i])
                if gx_hs[i] != 0
                else 0.0
            )
            g[HSINK * n + i][NL * n + SINK_C_W] = g[NL * n + SINK_C_W][
                HSINK * n + i
            ] = (
                2.0
                * border[i, 0]
                / ((1.0 / gx_hs[i]) + pk.r_hs1_x * gw_hs / gx_hs[i])
                if gx_hs[i] != 0
                else 0.0
            )

        pk = self.pack
        g[NL * n + SP_N][NL * n + SINK_C_N] = g[NL * n + SINK_C_N][
            NL * n + SP_N
        ] = (2.0 / pk.r_sp_per_y)
        g[NL * n + SP_S][NL * n + SINK_C_S] = g[NL * n + SINK_C_S][
            NL * n + SP_S
        ] = (2.0 / pk.r_sp_per_y)
        g[NL * n + SP_E][NL * n + SINK_C_E] = g[NL * n + SINK_C_E][
            NL * n + SP_E
        ] = (2.0 / pk.r_sp_per_x)
        g[NL * n + SP_W][NL * n + SINK_C_W] = g[NL * n + SINK_C_W][
            NL * n + SP_W
        ] = (2.0 / pk.r_sp_per_x)
        g[NL * n + SINK_C_N][NL * n + SINK_N] = g[NL * n + SINK_N][
            NL * n + SINK_C_N
        ] = 2.0 / (pk.r_hs + pk.r_hs2_y)
        g[NL * n + SINK_C_S][NL * n + SINK_S] = g[NL * n + SINK_S][
            NL * n + SINK_C_S
        ] = 2.0 / (pk.r_hs + pk.r_hs2_y)
        g[NL * n + SINK_C_E][NL * n + SINK_E] = g[NL * n + SINK_E][
            NL * n + SINK_C_E
        ] = 2.0 / (pk.r_hs + pk.r_hs2_x)
        g[NL * n + SINK_C_W][NL * n + SINK_W] = g[NL * n + SINK_W][
            NL * n + SINK_C_W
        ] = 2.0 / (pk.r_hs + pk.r_hs2_x)
        g_amb[n + SINK_C_N] = g_amb[n + SINK_C_S] = 1.0 / (
            pk.r_hs_c_per_y + pk.r_amb_c_per_y
        )
        g_amb[n + SINK_C_E] = g_amb[n + SINK_C_W] = 1.0 / (
            pk.r_hs_c_per_x + pk.r_amb_c_per_x
        )
        g_amb[n + SINK_N] = g_amb[n + SINK_S] = g_amb[n + SINK_E] = g_amb[
            n + SINK_W
        ] = 1.0 / (pk.r_hs_per + pk.r_amb_per)

        # Build B matrix
        b = np.zeros((m, m))
        for i in range(m):
            for j in range(i):
                if g[i][j] == 0.0 or g[j][i] == 0.0:
                    b[i][j] = b[j][i] = 0.0
                else:
                    b[i][j] = b[j][i] = -1.0 / (1.0 / g[i][j] + 1.0 / g[j][i])
        for i in range(m):
            if i >= HSINK * n and i < NL * n:
                b[i][i] = g_amb[i % n]
            elif i >= NL * n + SINK_C_W:
                b[i][i] = g_amb[n + i - NL * n]
            else:
                b[i][i] = 0.0
            for j in range(m):
                if i != j:
                    b[i][i] -= b[i][j]

        self._b = b
        self._g_amb = g_amb

    def _build_C_model(self):
        """Build capacitance. Implements populate_C_model_block()."""
        n = self.n_units
        m = self.n_nodes
        cfg = self.cfg
        flp = self.flp
        pk = self.pack
        t_chip = cfg.get("t_chip", 0.15e-3)
        p_chip = cfg.get("p_chip", 1.75e6)
        p_interface = cfg.get("p_interface", 4e6)
        p_spreader = cfg.get("p_spreader", 3.55e6)
        p_sink = cfg.get("p_sink", 3.55e6)
        t_interface = cfg.get("t_interface", 20e-6)
        t_spreader = cfg.get("t_spreader", 1e-3)
        t_sink = cfg.get("t_sink", 6.9e-3)
        c_convec = cfg.get("c_convec", 140.4)
        s_sink = cfg.get("s_sink", 0.06)

        a = np.zeros(m)
        for i in range(n):
            area = flp[i].height * flp[i].width
            a[i] = getcap(p_chip, t_chip, area)
            a[IFACE * n + i] = getcap(p_interface, t_interface, area)
            a[HSP * n + i] = getcap(p_spreader, t_spreader, area)
            c_amb = C_FACTOR * c_convec / (s_sink * s_sink) * area
            a[HSINK * n + i] = getcap(p_sink, t_sink, area) + c_amb
        a[NL * n + SP_N] = a[NL * n + SP_S] = pk.c_sp_per_y
        a[NL * n + SP_E] = a[NL * n + SP_W] = pk.c_sp_per_x
        a[NL * n + SINK_C_N] = a[NL * n + SINK_C_S] = (
            pk.c_hs_c_per_y + pk.c_amb_c_per_y
        )
        a[NL * n + SINK_C_E] = a[NL * n + SINK_C_W] = (
            pk.c_hs_c_per_x + pk.c_amb_c_per_x
        )
        a[NL * n + SINK_N] = a[NL * n + SINK_S] = a[NL * n + SINK_E] = a[
            NL * n + SINK_W
        ] = (pk.c_hs_per + pk.c_amb_per)

        self._a = a
        self._inva = 1.0 / a
        self._c_matrix = np.diag(self._inva) @ self._b

    def _set_internal_power(self, power: np.ndarray):
        """Set power for virtual nodes. Implements set_internal_power_block()."""
        n = self.n_units
        power[IFACE * n : (IFACE + 1) * n] = 0.0
        power[HSP * n : (HSP + 1) * n] = 0.0
        for i in range(n + EXTRA):
            power[HSINK * n + i] = self.ambient * self._g_amb[i]

    def _slope(self, y, p, dy):
        """dy = invA*p - C*y. Implements slope_fn_block()."""
        dy[:] = p - self._c_matrix @ y

    def steady_state(self, power_W: dict[str, float]) -> dict[str, float]:
        """Solve B*T = power. Implements steady_state_temp_block()."""
        n = self.n_units
        power = np.zeros(self.n_nodes)
        for i, u in enumerate(self.flp):
            power[i] = power_W.get(u.name, 0.0)
        self._set_internal_power(power)
        self.state[:] = np.linalg.solve(self._b, power)
        return {u.name: self.state[i] for i, u in enumerate(self.flp)}

    def _rk4_core_block(self, y, k1, p, h, yout):
        """Standard RK4 step for block model. Implements rk4_core() from RCutil.c."""
        t_rk = self._t_rk
        k2 = self._k2
        k3 = self._k3
        k4 = self._k4
        np.add(y, (h / 2.0) * k1, out=t_rk)
        self._slope(t_rk, p, k2)
        np.add(y, (h / 2.0) * k2, out=t_rk)
        self._slope(t_rk, p, k3)
        np.add(y, h * k3, out=t_rk)
        self._slope(t_rk, p, k4)
        yout[:] = y + (h / 6.0) * (k1 + 2.0 * k2 + 2.0 * k3 + k4)

    def step_rk4(
        self, power_W: dict[str, float], dt: float
    ) -> dict[str, float]:
        """Advance with adaptive RK4. Implements compute_temp_block()
        using the same rk4() from RCutil.c."""
        n = self.n_units
        power = np.zeros(self.n_nodes)
        for i, u in enumerate(self.flp):
            power[i] = power_W.get(u.name, 0.0)
        self._set_internal_power(power)
        p = self._inva * power

        k1 = self._k1
        ytemp = self._ytemp
        t1 = self._t1
        t2 = self._t2
        y = self.state

        t = 0.0
        new_h = MIN_STEP
        while t < dt and new_h >= MIN_STEP * DELTA:
            h = new_h
            self._slope(y, p, k1)  # compute k1 once before retry loop

            while True:
                self._rk4_core_block(y, k1, p, h, ytemp)
                self._rk4_core_block(y, k1, p, h / 2.0, t1)
                # Overwrite k1 with slope at midpoint — matches C code
                self._slope(t1, p, k1)
                self._rk4_core_block(t1, k1, p, h / 2.0, t2)

                mx = np.max(np.abs(ytemp - t2))
                if mx == 0.0:
                    mx = MIN_STEP

                if mx <= RK4_PRECISION:
                    new_h = RK4_SAFETY * h * (abs(RK4_PRECISION / mx) ** 0.2)
                    if new_h > RK4_MAXUP * h:
                        new_h = RK4_MAXUP * h
                else:
                    new_h = RK4_SAFETY * h * (abs(RK4_PRECISION / mx) ** 0.25)
                    if new_h < h / RK4_MAXDOWN:
                        new_h = h / RK4_MAXDOWN

                if new_h >= h:
                    break
                h = new_h
                # On retry, k1 is NOT recomputed — matches C code

            y[:] = ytemp
            new_h = min(new_h, dt - t - h)
            t += h

        return {u.name: self.state[i] for i, u in enumerate(self.flp)}

    def step_euler(
        self, power_W: dict[str, float], dt: float
    ) -> dict[str, float]:
        """One backward-Euler step for block model."""
        n = self.n_units
        power = np.zeros(self.n_nodes)
        for i, u in enumerate(self.flp):
            power[i] = power_W.get(u.name, 0.0)
        self._set_internal_power(power)

        if self._euler_dt != dt:
            C_diag = np.diag(self._a / dt)
            self._euler_A = C_diag + self._b
            self._euler_dt = dt

        rhs = (self._a / dt) * self.state + power
        self.state[:] = np.linalg.solve(self._euler_A, rhs)
        return {u.name: self.state[i] for i, u in enumerate(self.flp)}


# ---------------------------------------------------------------------------
# Public API — HotSpotSolver
# ---------------------------------------------------------------------------
class HotSpotSolver:
    """Python translation of the HotSpot thermal solver.
    Supports grid and block models with RK4, backward-Euler, and steady-state.
    """

    def __init__(
        self, floorplan: list[dict], layers: list[dict], config: dict
    ):
        self._flp_units = [
            FlpUnit(
                name=d["name"],
                width=d["width_m"],
                height=d["height_m"],
                leftx=d["x_m"],
                bottomy=d["y_m"],
                specificheat=d.get("specificheat", 0.0),
                resistivity=d.get("resistivity", 0.0),
                hasRes=d.get("hasRes", False),
                hasSh=d.get("hasSh", False),
            )
            for d in floorplan
        ]
        self._config = config
        model_type = config.get("model_type", "block")

        if model_type == "grid":
            layers_cfg = []
            for lc in layers:
                entry = {
                    "has_lateral": lc.get("has_lateral", True),
                    "has_power": lc.get("has_power", False),
                    "k": lc.get("k_W_mK", 100.0),
                    "thickness": lc.get("thickness_m", 0.15e-3),
                    "sp": lc.get("vol_heat_cap_J_m3K", 1.75e6),
                    "is_microchannel": lc.get("is_microchannel", False),
                }
                per_flp = lc.get("floorplan", None)
                if per_flp is not None:
                    entry["flp"] = [
                        FlpUnit(
                            name=d["name"],
                            width=d["width_m"],
                            height=d["height_m"],
                            leftx=d["x_m"],
                            bottomy=d["y_m"],
                            specificheat=d.get("specificheat", 0.0),
                            resistivity=d.get("resistivity", 0.0),
                            hasRes=d.get("hasRes", False),
                            hasSh=d.get("hasSh", False),
                        )
                        for d in per_flp
                    ]
                if (
                    lc.get("is_microchannel", False)
                    and "microchannel_config" in lc
                ):
                    mc_d = lc["microchannel_config"]
                    entry["mc_config"] = MicrochannelConfig(
                        cell_types=np.array(mc_d["cell_types"], dtype=int),
                        pumping_pressure=mc_d.get("pumping_pressure", 5000.0),
                        pump_internal_res=mc_d.get("pump_internal_res", 0.0),
                        inlet_temperature=mc_d.get("inlet_temperature", 300.0),
                        coolant_capac=mc_d.get("coolant_capac", 4172638.0),
                        coolant_res=mc_d.get("coolant_res", 1.647717911),
                        coolant_visc=mc_d.get("coolant_visc", 0.000889),
                        wall_capac=mc_d.get("wall_capac", 1635660.0),
                        wall_res=mc_d.get("wall_res", 0.0076923077),
                        htc=mc_d.get("htc", 27132.0),
                        cell_width=mc_d.get("cell_width", 100e-6),
                        cell_height=mc_d.get("cell_height", 100e-6),
                        cell_thickness=mc_d.get("cell_thickness", 100e-6),
                    )
                layers_cfg.append(entry)
            cfg = self._translate_config(config)
            self._model = _GridModel(self._flp_units, layers_cfg, cfg)
        else:
            cfg = self._translate_config(config)
            self._model = _BlockModel(self._flp_units, cfg)

        self._model_type = model_type

    @staticmethod
    def _translate_config(config: dict) -> dict:
        """Map the public config dict keys to internal names matching C fields."""
        c = {}
        mapping = {
            "chip_width_m": None,
            "chip_height_m": None,
            "t_chip": ("t_chip", 0.15e-3),
            "k_chip": ("k_chip", 100.0),
            "p_chip": ("p_chip", 1.75e6),
            "s_sink": ("s_sink", 0.06),
            "t_sink": ("t_sink", 6.9e-3),
            "k_sink": ("k_sink", 400.0),
            "p_sink": ("p_sink", 3.55e6),
            "s_spreader": ("s_spreader", 0.03),
            "t_spreader": ("t_spreader", 1e-3),
            "k_spreader": ("k_spreader", 400.0),
            "p_spreader": ("p_spreader", 3.55e6),
            "t_interface": ("t_interface", 20e-6),
            "k_interface": ("k_interface", 4.0),
            "p_interface": ("p_interface", 4e6),
            "r_convec": ("r_convec", 0.1),
            "c_convec": ("c_convec", 140.4),
            "ambient": ("ambient", 318.15),
            "init_temp": ("init_temp", 333.15),
        }
        # Direct key copies with name translations
        key_map = {
            "spreader_width_m": "s_spreader",
            "spreader_height_m": "s_spreader",
            "spreader_thickness_m": "t_spreader",
            "spreader_k_W_mK": "k_spreader",
            "spreader_vol_heat_cap_J_m3K": "p_spreader",
            "heatsink_width_m": "s_sink",
            "heatsink_height_m": "s_sink",
            "heatsink_thickness_m": "t_sink",
            "heatsink_k_W_mK": "k_sink",
            "heatsink_vol_heat_cap_J_m3K": "p_sink",
            "chip_width_m": "chip_width",
            "chip_height_m": "chip_height",
            "r_convec_K_per_W": "r_convec",
            "c_convec_J_per_K": "c_convec",
            "ambient_temp_K": "ambient",
            "initial_temp_K": "init_temp",
            "grid_rows": "grid_rows",
            "grid_cols": "grid_cols",
            "model_secondary": "model_secondary",
            "detailed_3D_used": "detailed_3D_used",
            "grid_map_mode": "grid_map_mode",
            "model_type": "model_type",
            "block_omit_lateral": "block_omit_lateral",
            "sub_thickness_m": "t_sub",
            "sub_k_W_mK": "k_sub_user",
            "sub_vol_heat_cap_J_m3K": "sp_sub_user",
            "solder_thickness_m": "t_solder",
            "solder_k_W_mK": "k_solder_user",
            "solder_vol_heat_cap_J_m3K": "sp_solder_user",
            "pcb_thickness_m": "t_pcb",
            "pcb_k_W_mK": "k_pcb_user",
            "pcb_vol_heat_cap_J_m3K": "sp_pcb_user",
            "r_convec_sec_K_per_W": "r_convec_sec",
            "c_convec_sec_J_per_K": "c_convec_sec",
            "s_sub": "s_sub",
            "t_sub": "t_sub",
            "s_solder": "s_solder",
            "t_solder": "t_solder",
            "s_pcb": "s_pcb",
            "t_pcb": "t_pcb",
            "t_chip": "t_chip",
            "k_chip": "k_chip",
            "p_chip": "p_chip",
            "t_interface": "t_interface",
            "k_interface": "k_interface",
            "p_interface": "p_interface",
            "t_spreader": "t_spreader",
            "k_spreader": "k_spreader",
            "p_spreader": "p_spreader",
            "t_sink": "t_sink",
            "k_sink": "k_sink",
            "p_sink": "p_sink",
            "s_spreader": "s_spreader",
            "s_sink": "s_sink",
            "r_convec": "r_convec",
            "c_convec": "c_convec",
            "ambient": "ambient",
            "init_temp": "init_temp",
            "r_convec_sec": "r_convec_sec",
            "c_convec_sec": "c_convec_sec",
            "sampling_intvl": "sampling_intvl",
            "t_metal": "t_metal",
            "t_c4": "t_c4",
        }
        for ext_key, int_key in key_map.items():
            if ext_key in config:
                c[int_key] = config[ext_key]
        # Defaults for secondary path
        c.setdefault("s_sub", 0.021)
        c.setdefault("t_sub", 0.001)
        c.setdefault("s_solder", 0.021)
        c.setdefault("t_solder", 0.00094)
        c.setdefault("s_pcb", 0.1)
        c.setdefault("t_pcb", 0.002)
        c.setdefault("r_convec_sec", 1.0)
        c.setdefault("c_convec_sec", 140.4)
        c.setdefault("t_metal", 10e-6)
        c.setdefault("t_c4", 0.0001)
        c.setdefault("model_secondary", False)
        c.setdefault("detailed_3D_used", False)
        c.setdefault("grid_map_mode", "center")
        c.setdefault("grid_rows", 64)
        c.setdefault("grid_cols", 64)
        c.setdefault("block_omit_lateral", False)
        # Core defaults
        c.setdefault("t_chip", 0.15e-3)
        c.setdefault("k_chip", 100.0)
        c.setdefault("p_chip", 1.75e6)
        c.setdefault("t_interface", 20e-6)
        c.setdefault("k_interface", 4.0)
        c.setdefault("p_interface", 4e6)
        c.setdefault("t_spreader", 1e-3)
        c.setdefault("k_spreader", 400.0)
        c.setdefault("p_spreader", 3.55e6)
        c.setdefault("t_sink", 6.9e-3)
        c.setdefault("k_sink", 400.0)
        c.setdefault("p_sink", 3.55e6)
        c.setdefault("s_spreader", 0.03)
        c.setdefault("s_sink", 0.06)
        c.setdefault("r_convec", 0.1)
        c.setdefault("c_convec", 140.4)
        c.setdefault("ambient", 318.15)
        c.setdefault("init_temp", 333.15)
        return c

    def step(
        self, power_W: dict[str, float], dt_s: float, method: str = "rk4"
    ) -> dict[str, float]:
        """Advance thermal state by dt_s seconds."""
        if method == "rk4":
            return self._model.step_rk4(power_W, dt_s)
        elif method == "euler":
            return self._model.step_euler(power_W, dt_s)
        else:
            raise ValueError(f"Unknown method: {method}")

    def steady_state(self, power_W: dict[str, float]) -> dict[str, float]:
        """Compute steady-state temperatures."""
        return self._model.steady_state(power_W)

    def reset(self, initial_temp_K: float | None = None) -> None:
        """Reset all node temperatures."""
        t = (
            initial_temp_K
            if initial_temp_K is not None
            else self._model.init_temp
        )
        self._model.state[:] = t

    @property
    def state_vector(self) -> np.ndarray:
        """Full ODE state vector."""
        return self._model.state.copy()

    @state_vector.setter
    def state_vector(self, v: np.ndarray) -> None:
        self._model.state[:] = v
