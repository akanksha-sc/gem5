#include "sim/power/thermal_model_pyfunc.hh"

namespace gem5
{

ThermalModelPyFunc::ThermalModelPyFunc(const Params &p)
    : SimObject(p), solve_obj(p.solve), reset_obj(p.reset)
{
    solve_func = pybind11::reinterpret_borrow<pybind11::function>(solve_obj);
    reset_func = pybind11::reinterpret_borrow<pybind11::function>(reset_obj);
}

pybind11::object
ThermalModelPyFunc::solve(const pybind11::list &domain_data, double dt_seconds)
{
    pybind11::gil_scoped_acquire acquire;
    pybind11::object result = solve_func(domain_data, dt_seconds);

    if (pybind11::isinstance<pybind11::tuple>(result) &&
        pybind11::len(result) == 2) {
        pybind11::tuple rich = result.cast<pybind11::tuple>();
        last_solver_substeps = rich[1].cast<int>();
        return rich[0];
    }

    last_solver_substeps = 1;
    return result;
}

void
ThermalModelPyFunc::reset(double initial_temp_k)
{
    pybind11::gil_scoped_acquire acquire;
    reset_func(initial_temp_k);
}

} // namespace gem5
