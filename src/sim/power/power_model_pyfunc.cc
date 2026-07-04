#include "sim/power/power_model_pyfunc.hh"

#include <cmath>
#include <fstream>
#include <iomanip>

#include "base/logging.hh"
#include "base/trace.hh"
#include "debug/PwrIntervalEvent.hh"
#include "sim/clocked_object.hh"

namespace gem5
{

PowerModelPyFunc::PowerModelPyFunc(const Params &p)
    : PowerModelState(p),
      dyn(p.dyn),
      st(p.st),
      pwr_interval(p.pwr_interval),
      pwr_interval_ticks(p.pwr_interval_ticks),
      enable_trace(p.enable_trace),
      trace_file(p.trace_file),
      trace_label(p.trace_label),
      intervalEvent([this] { powerAtInterval(); }, name()),
      stats(this)
{
    dyn_func = pybind11::reinterpret_borrow<pybind11::function>(dyn);
    st_func = pybind11::reinterpret_borrow<pybind11::function>(st);
    reset_func = pybind11::reinterpret_borrow<pybind11::function>(p.reset);
}

void
PowerModelPyFunc::startup()
{
    // ROI-controlled sampling; do not auto-start even if auto_start is set.
}

double
PowerModelPyFunc::getDynamicPower() const
{
    pybind11::gil_scoped_acquire acquire;
    pybind11::object result_py = dyn_func();
    return result_py.cast<double>();
}

double
PowerModelPyFunc::getStaticPower() const
{
    pybind11::gil_scoped_acquire acquire;
    pybind11::object result_py = st_func();
    return result_py.cast<double>();
}

void
PowerModelPyFunc::clearCachedSample()
{
    has_sampled_power = false;
    cached_dynamic_power = 0.0;
    cached_static_power = 0.0;
    cached_total_power = 0.0;
    cached_power_tick = 0;
    cached_temperature_k = 0.0;
    cached_sample_duration_ticks = 0;
}

void
PowerModelPyFunc::clearAccumulatedPower()
{
    accum_dynamic_power_tick_sum = 0.0;
    accum_static_power_tick_sum = 0.0;
    accum_duration_ticks = 0;
    accum_end_tick = 0;
    accum_sample_count = 0;
}

double
PowerModelPyFunc::getAccumulatedDynamicPower() const
{
    return accum_duration_ticks == 0
               ? 0.0
               : accum_dynamic_power_tick_sum /
                     static_cast<double>(accum_duration_ticks);
}

double
PowerModelPyFunc::getAccumulatedStaticPower() const
{
    return accum_duration_ticks == 0
               ? 0.0
               : accum_static_power_tick_sum /
                     static_cast<double>(accum_duration_ticks);
}

double
PowerModelPyFunc::getAccumulatedTotalPower() const
{
    return getAccumulatedDynamicPower() + getAccumulatedStaticPower();
}

Tick
PowerModelPyFunc::getAccumulatedPowerTick() const
{
    return accum_end_tick;
}

Tick
PowerModelPyFunc::getAccumulatedPowerDurationTicks() const
{
    return accum_duration_ticks;
}

uint64_t
PowerModelPyFunc::getAccumulatedPowerSampleCount() const
{
    return accum_sample_count;
}

double
PowerModelPyFunc::sanitizePowerValue(double value,
                                     const std::string &kind) const
{
    if (!std::isfinite(value)) {
        panic("PowerModelPyFunc %s produced non-finite %s power: %g\n", name(),
              kind.c_str(), value);
    }

    constexpr double kEpsilon = 1e-12;
    if (value < 0.0) {
        if (std::abs(value) < kEpsilon) {
            DPRINTF(PwrIntervalEvent,
                    "PowerModelPyFunc %s clamped tiny negative %s power "
                    "%g to 0\n",
                    name(), kind.c_str(), value);
            return 0.0;
        }
        panic("PowerModelPyFunc %s produced negative %s power: %g\n", name(),
              kind.c_str(), value);
    }

    return value;
}

Tick
PowerModelPyFunc::computeEffectiveIntervalTicks() const
{
    if (pwr_interval_ticks > 0) {
        return pwr_interval_ticks;
    }

    if (pwr_interval == 0) {
        return 0;
    }

    panic_if(!clocked_object,
             "PowerModelPyFunc %s has no attached ClockedObject and no "
             "absolute pwr_interval_ticks was provided.\n",
             name());

    return pwr_interval * clocked_object->clockPeriod();
}

std::string
PowerModelPyFunc::traceName() const
{
    if (!trace_label.empty()) {
        return trace_label;
    }

    if (clocked_object) {
        return std::string(clocked_object->name());
    }

    return name();
}

void
PowerModelPyFunc::startSampling()
{
    if (sampling_active) {
        return;
    }

    effective_interval_ticks = computeEffectiveIntervalTicks();
    panic_if(effective_interval_ticks == 0,
             "PowerModelPyFunc %s started with zero effective interval.\n",
             name());

    clearCachedSample();
    clearAccumulatedPower();

    {
        pybind11::gil_scoped_acquire acquire;
        reset_func();
    }

    sampling_active = true;
    last_sample_boundary_tick = curTick();

    DPRINTF(PwrIntervalEvent, "Starting sampling of SO: %s\n",
            traceName().c_str());

    schedule(intervalEvent, curTick() + effective_interval_ticks);
}

void
PowerModelPyFunc::stopSampling()
{
    if (!sampling_active) {
        return;
    }

    DPRINTF(PwrIntervalEvent, "Stopping sampling of SO: %s\n",
            traceName().c_str());

    if (intervalEvent.scheduled()) {
        deschedule(intervalEvent);
    }

    sampling_active = false;
}

void
PowerModelPyFunc::appendTraceSample(double dyn, double st, double total_power,
                                    Tick duration_ticks, double temp_k,
                                    const char *sample_kind) const
{
    if (!enable_trace || trace_file.empty()) {
        DPRINTF(PwrIntervalEvent,
                "Trace disabled for %s because trace_file is empty\n",
                traceName().c_str());
        return;
    }

    std::ofstream csv(trace_file, std::ios::app);
    if (!csv.is_open()) {
        DPRINTF(PwrIntervalEvent, "Could not open trace_file=%s for %s\n",
                trace_file.c_str(), traceName().c_str());
        return;
    }

    if (csv.tellp() == 0) {
        csv << "tick,label,dynamic_power_w,static_power_w,total_power_w,"
               "sample_duration_ticks,sample_duration_s,temp_k,sample_kind\n";
    }

    const double duration_s = static_cast<double>(duration_ticks) /
                              static_cast<double>(sim_clock::as_int::s);

    csv << std::setprecision(17);

    csv << curTick() << "," << traceName() << "," << dyn << "," << st << ","
        << total_power << "," << duration_ticks << "," << duration_s << ","
        << temp_k << "," << sample_kind << "\n";

    DPRINTF(PwrIntervalEvent, "Appended trace sample for %s to %s\n",
            traceName().c_str(), trace_file.c_str());
}

void
PowerModelPyFunc::doSample(bool reschedule_next, const char *sample_kind)
{
    Tick now = curTick();
    active_sample_duration_ticks = now - last_sample_boundary_tick;

    if (active_sample_duration_ticks == 0) {
        if (reschedule_next) {
            panic("PowerModelPyFunc %s interval sample at tick %llu has "
                  "zero duration (scheduling corruption).\n",
                  name(), now);
        }
        DPRINTF(PwrIntervalEvent,
                "Skipping zero-duration flush sample for %s at tick %llu\n",
                traceName().c_str(), now);
        return;
    }

    last_sample_boundary_tick = now;

    DPRINTF(PwrIntervalEvent, "Sampling %s at tick %llu (dur=%llu ticks)\n",
            traceName().c_str(), now, active_sample_duration_ticks);

    in_power_interval = true;

    double dyn = 0.0;
    double st = 0.0;
    try {
        dyn = getDynamicPower();
        st = getStaticPower();
    } catch (...) {
        in_power_interval = false;
        throw;
    }

    in_power_interval = false;

    dyn = sanitizePowerValue(dyn, "dynamic");
    st = sanitizePowerValue(st, "static");
    double total_power = sanitizePowerValue(dyn + st, "total");

    cached_dynamic_power = dyn;
    cached_static_power = st;
    cached_total_power = total_power;
    cached_power_tick = now;
    cached_temperature_k = _temp.toKelvin();
    cached_sample_duration_ticks = active_sample_duration_ticks;
    has_sampled_power = true;

    accum_dynamic_power_tick_sum += dyn * active_sample_duration_ticks;
    accum_static_power_tick_sum += st * active_sample_duration_ticks;
    accum_duration_ticks += active_sample_duration_ticks;
    accum_end_tick = now;
    accum_sample_count++;

    stats.powerDist.sample(total_power, 1);
    stats.dynamicPowerDist.sample(dyn, 1);
    stats.staticPowerDist.sample(st, 1);
    appendTraceSample(dyn, st, total_power, active_sample_duration_ticks,
                      cached_temperature_k, sample_kind);

    if (reschedule_next && sampling_active) {
        schedule(intervalEvent, curTick() + effective_interval_ticks);
    }
}

void
PowerModelPyFunc::powerAtInterval()
{
    assert(effective_interval_ticks > 0 && sampling_active);
    doSample(true, "interval");
}

void
PowerModelPyFunc::sampleNow()
{
    panic_if(!sampling_active,
             "PowerModelPyFunc %s sampleNow() called before "
             "startSampling().\n",
             name());

    if (intervalEvent.scheduled()) {
        deschedule(intervalEvent);
    }

    doSample(false, "flush");
}

} // namespace gem5
