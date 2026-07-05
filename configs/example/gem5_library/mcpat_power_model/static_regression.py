import math

# Minimal subset needed for the runtime-sampled top-level thermal domains.
# Form: P_static(T_K) = exp(m * T_K + b)

INORDER_REGRESSION = {
    "Core": (1.5090e-02, -9.1808),
    "Instruction Cache": (1.5090e-02, -11.2378),
    "Data Cache": (1.5090e-02, -11.2212),
    "L2": (2.3867e-02, -9.4142),
}


O3_REGRESSION = {
    "Core": (1.5090e-02, -8.9887),
    "Instruction Cache": (1.5090e-02, -11.0684),
    "Data Cache": (1.5090e-02, -11.0519),
    "L2": (2.3867e-02, -9.4142),
}


def leakage_w(
    regression,
    component,
    temp_k,
    min_temp_k=250.0,
    max_temp_k=450.0,
    min_exp=-80.0,
    max_exp=80.0,
):
    if component not in regression:
        raise KeyError(f"Missing static regression entry for '{component}'")

    m, b = regression[component]

    temp_k = float(temp_k)
    temp_k = max(min_temp_k, min(max_temp_k, temp_k))

    exponent = m * temp_k + b
    exponent = max(min_exp, min(max_exp, exponent))

    return math.exp(exponent)
