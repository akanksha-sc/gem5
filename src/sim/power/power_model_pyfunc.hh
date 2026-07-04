#ifndef __SIM_POWERMODEL_FUNC_PM_HH__
#define __SIM_POWERMODEL_FUNC_PM_HH__

#include <cmath>
#include <string>

#include "base/compiler.hh"
#include "base/statistics.hh"
#include "params/PowerModelPyFunc.hh"
#include "python/pybind11/pybind.hh"
#include "sim/core.hh"
#include "sim/power/power_model.hh"
#include "sim/sim_object.hh"

namespace gem5
{

class GEM5_LOCAL PowerModelPyFunc : public PowerModelState
{
  private:
    pybind11::object dyn;
    pybind11::object st;
    const Cycles pwr_interval;
    const Tick pwr_interval_ticks;
    const bool enable_trace;
    std::string trace_file;
    std::string trace_label;

    Tick effective_interval_ticks = 0;
    pybind11::function st_func;
    pybind11::function dyn_func;
    pybind11::function reset_func;
    Tick last_sample_boundary_tick = 0;
    Tick active_sample_duration_ticks = 0;
    Tick cached_sample_duration_ticks = 0;

    bool sampling_active = false;
    bool has_sampled_power = false;
    bool in_power_interval = false;

    double cached_dynamic_power = 0.0;
    double cached_static_power = 0.0;
    double cached_total_power = 0.0;
    double cached_temperature_k = 0.0;
    Tick cached_power_tick = 0;

    double accum_dynamic_power_tick_sum = 0.0;
    double accum_static_power_tick_sum = 0.0;
    Tick accum_duration_ticks = 0;
    Tick accum_end_tick = 0;
    uint64_t accum_sample_count = 0;

    EventFunctionWrapper intervalEvent;
    void powerAtInterval();
    void doSample(bool reschedule_next, const char *sample_kind);
    double sanitizePowerValue(double value, const std::string &kind) const;
    void appendTraceSample(double dyn, double st, double total_power,
                           Tick duration_ticks, double temp_k,
                           const char *sample_kind) const;

  public:
    PARAMS(PowerModelPyFunc);
    PowerModelPyFunc(const Params &p);
    void startup() override;
    double getDynamicPower() const override;
    double getStaticPower() const override;

    void startSampling();
    void stopSampling();
    bool
    isSamplingActive() const
    {
        return sampling_active;
    }
    bool
    inPowerAtInterval() const
    {
        return in_power_interval;
    }

    void clearCachedSample() override;
    void clearAccumulatedPower() override;

    double
    getCachedDynamicPower() const override
    {
        return cached_dynamic_power;
    }
    double
    getCachedStaticPower() const override
    {
        return cached_static_power;
    }
    double
    getCachedTotalPower() const
    {
        return cached_total_power;
    }
    Tick
    getCachedPowerTick() const override
    {
        return cached_power_tick;
    }

    double
    getCachedTemperatureKelvin() const override
    {
        return cached_temperature_k;
    }

    double getAccumulatedDynamicPower() const override;
    double getAccumulatedStaticPower() const override;
    double getAccumulatedTotalPower() const override;
    Tick getAccumulatedPowerTick() const override;
    Tick getAccumulatedPowerDurationTicks() const override;
    uint64_t getAccumulatedPowerSampleCount() const override;

    double
    getTemperatureKelvin() const
    {
        return _temp.toKelvin();
    }

    void sampleNow();

    Tick computeEffectiveIntervalTicks() const;
    Tick
    getEffectiveIntervalTicks() const
    {
        return effective_interval_ticks;
    }
    std::string traceName() const;

    double
    getSampleDurationSeconds() const
    {
        Tick dur = in_power_interval ? active_sample_duration_ticks
                                     : cached_sample_duration_ticks;
        return static_cast<double>(dur) /
               static_cast<double>(sim_clock::as_int::s);
    }

    struct powerModelStats : public statistics::Group
    {
        statistics::Histogram powerDist;
        statistics::Histogram dynamicPowerDist;
        statistics::Histogram staticPowerDist;
        powerModelStats(statistics::Group *parent)
            : statistics::Group(parent),
              ADD_STAT(powerDist, "Total Power Sampled Statistics"),
              ADD_STAT(dynamicPowerDist, "Dynamic Power Sampled Statistics"),
              ADD_STAT(staticPowerDist, "Static Power Sampled Statistics")
        {
            powerDist.init(2);
            dynamicPowerDist.init(2);
            staticPowerDist.init(2);
        }
    };

    powerModelStats stats;
};

} // namespace gem5

#endif
