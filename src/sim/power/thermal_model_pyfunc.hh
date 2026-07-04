#ifndef __SIM_THERMAL_MODEL_PYFUNC_HH__
#define __SIM_THERMAL_MODEL_PYFUNC_HH__

#include "base/compiler.hh"
#include "params/ThermalModelPyFunc.hh"
#include "python/pybind11/pybind.hh"
#include "sim/sim_object.hh"

namespace gem5
{

class GEM5_LOCAL ThermalModelPyFunc : public SimObject
{
  private:
    pybind11::object solve_obj;
    pybind11::function solve_func;

    pybind11::object reset_obj;
    pybind11::function reset_func;

    int last_solver_substeps = 1;

  public:
    PARAMS(ThermalModelPyFunc);
    ThermalModelPyFunc(const Params &p);

    pybind11::object solve(const pybind11::list &domain_data,
                           double dt_seconds);

    void reset(double initial_temp_k);

    int
    getLastSolverSubsteps() const
    {
        return last_solver_substeps;
    }
};

} // namespace gem5

#endif
