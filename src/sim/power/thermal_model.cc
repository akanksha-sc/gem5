/*
 * Copyright (c) 2015, 2021 Arm Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "sim/power/thermal_model.hh"

#include <fstream>
#include <iomanip>
#include <stdexcept>

#include "base/statistics.hh"
#include "base/trace.hh"
#include "debug/ThermalIntervalEvent.hh"
#include "params/ThermalCapacitor.hh"
#include "params/ThermalModel.hh"
#include "params/ThermalReference.hh"
#include "params/ThermalResistor.hh"
#include "sim/clocked_object.hh"
#include "sim/linear_solver.hh"
#include "sim/power/thermal_domain.hh"
#include "sim/power/thermal_model_pyfunc.hh"
#include "sim/sim_object.hh"

namespace gem5
{

/**
 * ThermalReference
 */
ThermalReference::ThermalReference(const Params &p)
    : SimObject(p), _temperature(p.temperature), node(NULL)
{
}

LinearEquation
ThermalReference::getEquation(ThermalNode * n, unsigned nnodes,
                              double step) const {
    // Just return an empty equation
    return LinearEquation(nnodes);
}

/**
 * ThermalResistor
 */
ThermalResistor::ThermalResistor(const Params &p)
    : SimObject(p), _resistance(p.resistance), node1(NULL), node2(NULL)
{
}

LinearEquation
ThermalResistor::getEquation(ThermalNode * n, unsigned nnodes,
                             double step) const
{
    // i[n] = (Vn2 - Vn1)/R
    LinearEquation eq(nnodes);

    if (n != node1 && n != node2)
        return eq;

    if (node1->isref)
        eq[eq.cnt()] += -node1->temp.toKelvin() / _resistance;
    else
        eq[node1->id] += -1.0f / _resistance;

    if (node2->isref)
        eq[eq.cnt()] += node2->temp.toKelvin() / _resistance;
    else
        eq[node2->id] += 1.0f / _resistance;

    // We've assumed n was node1, reverse if necessary
    if (n == node2)
        eq *= -1.0f;

    return eq;
}

/**
 * ThermalCapacitor
 */
ThermalCapacitor::ThermalCapacitor(const Params &p)
    : SimObject(p), _capacitance(p.capacitance), node1(NULL), node2(NULL)
{
}

LinearEquation
ThermalCapacitor::getEquation(ThermalNode * n, unsigned nnodes,
                              double step) const
{
    // i(t) = C * d(Vn2 - Vn1)/dt
    // i[n] = C/step * (Vn2 - Vn1 - Vn2[n-1] + Vn1[n-1])
    LinearEquation eq(nnodes);

    if (n != node1 && n != node2)
        return eq;

    eq[eq.cnt()] += _capacitance / step *
        (node1->temp - node2->temp).toKelvin();

    if (node1->isref)
        eq[eq.cnt()] += _capacitance / step * (-node1->temp.toKelvin());
    else
        eq[node1->id] += -1.0f * _capacitance / step;

    if (node2->isref)
        eq[eq.cnt()] += _capacitance / step * (node2->temp.toKelvin());
    else
        eq[node2->id] += 1.0f * _capacitance / step;

    // We've assumed n was node1, reverse if necessary
    if (n == node2)
        eq *= -1.0f;

    return eq;
}

/**
 * ThermalModel
 */
ThermalModel::ThermalModel(const Params &p)
    : ClockedObject(p),
      stepEvent([this] { doStep(); }, name()),
      _step(p.step),
      thermal_interval(p.thermal_interval),
      thermal_interval_ticks(p.thermal_interval_ticks),
      auto_start(p.auto_start),
      sample_wait_timeout_ticks(p.sample_wait_timeout_ticks),
      thermal_post_power_delay_ticks(p.thermal_post_power_delay_ticks),
      solver(p.solver),
      enable_trace(p.enable_trace),
      trace_file(p.trace_file),
      intervalStepEvent([this] { stepAtInterval(); }, name()),
      ADD_STAT(thermalStepCount,
               "Number of successful thermal steps (interval + flush)"),
      ADD_STAT(thermalIntervalStepCount, "Number of interval thermal steps"),
      ADD_STAT(thermalFlushStepCount, "Number of flush thermal steps"),
      ADD_STAT(thermalWaitRetryCount,
               "Number of one-tick waits for aligned power samples"),
      ADD_STAT(lastThermalStepTick, statistics::units::Tick::get(),
               "Tick of last successful thermal step"),
      ADD_STAT(lastConsumedPowerTick, statistics::units::Tick::get(),
               "Power sample tick consumed by last thermal step"),
      ADD_STAT(lastStepDtTicks, statistics::units::Tick::get(),
               "Thermal integration dt for last step in ticks"),
      ADD_STAT(lastStepDtSeconds, statistics::units::Second::get(),
               "Thermal integration dt for last step in seconds")
{
    thermalStepCount.functor([this]() -> double { return numThermalSteps; });
    thermalIntervalStepCount.functor(
        [this]() -> double { return numThermalIntervalSteps; });
    thermalFlushStepCount.functor(
        [this]() -> double { return numThermalFlushSteps; });
    thermalWaitRetryCount.functor(
        [this]() -> double { return numThermalWaitRetries; });
    lastThermalStepTick.functor(
        [this]() -> double { return lastThermalStepTickValue; });
    lastConsumedPowerTick.functor(
        [this]() -> double { return lastConsumedPowerTickValue; });
    lastStepDtTicks.functor(
        [this]() -> double { return lastStepDtTicksValue; });
    lastStepDtSeconds.functor(
        [this]() -> double { return lastStepDtSecondsValue; });
}

Tick
ThermalModel::computeEffectiveIntervalTicks() const
{
    if (thermal_interval_ticks > 0) {
        return thermal_interval_ticks;
    }

    if (thermal_interval == 0) {
        return 0;
    }

    return thermal_interval * clockPeriod();
}

void
ThermalModel::doStep()
{
    // Calculate new temperatures!
    // For each node in the system, create the kirchhoff nodal equation
    LinearSystem ls(eq_nodes.size());
    for (unsigned i = 0; i < eq_nodes.size(); i++) {
        auto n = eq_nodes[i];
        LinearEquation node_equation (eq_nodes.size());
        for (auto e : entities) {
            LinearEquation eq = e->getEquation(n, eq_nodes.size(), _step);
            node_equation = node_equation + eq;
        }
        ls[i] = node_equation;
    }

    // Get temperatures for this iteration
    std::vector <double> temps = ls.solve();
    for (unsigned i = 0; i < eq_nodes.size(); i++)
        eq_nodes[i]->temp = Temperature::fromKelvin(temps[i]);

    // Schedule next computation
    schedule(stepEvent, curTick() + sim_clock::as_int::s * _step);

    // Notify everybody
    for (auto dom : domains)
        dom->emitUpdate();
}

void
ThermalModel::startup()
{
    // Look for nodes connected to voltage references, these
    // can be just set to the reference value (no nodal equation)
    for (auto ref : references) {
        ref->node->temp = ref->_temperature;
        ref->node->isref = true;
    }
    // Setup the initial temperatures.
    for (auto dom : domains) {
        dom->getNode()->temp = dom->initialTemperature();
    }

    // Synchronize all temperature-dependent power models with the domain's
    // initial temperature before any sampled power query can occur.
    //
    // Without this, the first ROI power sample may still use the power
    // model's ambient default temperature (e.g., 25C / 298.15K) instead of
    // the thermal domain initial temperature (e.g., 300K).
    for (auto dom : domains)
        dom->emitUpdate();

    // Create a list of unknown temperature nodes
    for (auto n : nodes) {
        bool found = false;
        for (auto ref : references)
            if (ref->node == n) {
                found = true;
                break;
            }
        if (!found)
            eq_nodes.push_back(n);
    }

    // Assign each node an ID
    for (unsigned i = 0; i < eq_nodes.size(); i++)
        eq_nodes[i]->id = i;

    if (intervalSteppingConfigured()) {
        // ROI-controlled stepping; do not auto-start.
    } else {
        // Schedule first native RC thermal update.
        schedule(stepEvent, curTick() + sim_clock::as_int::s * _step);
    }
}

void
ThermalModel::addDomain(ThermalDomain * d)
{
    domains.push_back(d);
    entities.push_back(d);
}

void
ThermalModel::addReference(ThermalReference * r)
{
    references.push_back(r);
    entities.push_back(r);
}

void
ThermalModel::addCapacitor(ThermalCapacitor * c)
{
    capacitors.push_back(c);
    entities.push_back(c);
}

void
ThermalModel::addResistor(ThermalResistor * r)
{
    resistors.push_back(r);
    entities.push_back(r);
}

Temperature
ThermalModel::getTemperature() const
{
    // Just pick the highest temperature
    Temperature temp = Temperature::fromKelvin(0.0);
    for (auto & n : eq_nodes)
        temp = std::max(temp, n->temp);
    return temp;
}

void
ThermalModel::startStepping()
{
    intervalTicks = computeEffectiveIntervalTicks();
    if (intervalTicks == 0)
        return;

    if (!solver) {
        warn("ThermalModel::startStepping: no solver configured\n");
        return;
    }

    if (domains.empty()) {
        warn("ThermalModel::startStepping: no thermal domains registered\n");
        return;
    }

    if (stepping_active)
        return;

    stepping_active = true;
    nextIntervalStepTick =
        curTick() + intervalTicks + thermal_post_power_delay_ticks;
    lastConsumedPowerTickValue = 0;
    hasConsumedPowerSample = false;
    steppingStartTick = curTick();
    waitStartTick = 0;

    numThermalSteps = 0;
    numThermalIntervalSteps = 0;
    numThermalFlushSteps = 0;
    numThermalWaitRetries = 0;
    lastThermalStepTickValue = 0;
    lastStepDtTicksValue = 0;
    lastStepDtSecondsValue = 0.0;
    lastStepKind = "none";

    solver->reset(domains.front()->currentTemperature().toKelvin());

    DPRINTF(ThermalIntervalEvent,
            "Starting thermal stepping at tick %llu; first step at %llu\n",
            curTick(), nextIntervalStepTick);

    schedule(intervalStepEvent, nextIntervalStepTick);
}

void
ThermalModel::stopStepping()
{
    if (!stepping_active)
        return;

    DPRINTF(ThermalIntervalEvent, "Stopping thermal stepping at tick %llu\n",
            curTick());

    if (intervalStepEvent.scheduled())
        deschedule(intervalStepEvent);

    stepping_active = false;
    nextIntervalStepTick = 0;
    steppingStartTick = 0;
}

bool
ThermalModel::consumeSampledPower(bool reschedule_next)
{
    if (!solver || domains.empty()) {
        return false;
    }

    Tick accum_end_tick = 0;
    Tick accum_duration_ticks = 0;
    uint64_t accum_sample_count = 0;

    for (auto *dom : domains) {
        uint64_t count = dom->getAccumulatedPowerSampleCount();
        Tick duration = dom->getAccumulatedPowerDurationTicks();
        Tick end_tick = dom->getAccumulatedPowerTick();

        if (count == 0 || duration == 0) {
            return false;
        }

        if (accum_end_tick == 0) {
            accum_end_tick = end_tick;
            accum_duration_ticks = duration;
            accum_sample_count = count;
        } else if (accum_end_tick != end_tick ||
                   accum_duration_ticks != duration ||
                   accum_sample_count != count) {
            return false;
        }
    }

    if (reschedule_next) {
        Tick expected_end =
            nextIntervalStepTick - thermal_post_power_delay_ticks;
        if (accum_end_tick != expected_end) {
            return false;
        }
    }

    const double dt_s = static_cast<double>(accum_duration_ticks) /
                        static_cast<double>(sim_clock::as_int::s);

    lastStepDtTicksValue = accum_duration_ticks;
    lastStepDtSecondsValue = dt_s;
    lastThermalStepTickValue = curTick();
    lastStepKind = reschedule_next ? "interval" : "flush";

    traceInputTempsK.clear();
    traceInputTempsK.reserve(domains.size());
    for (auto *dom : domains) {
        traceInputTempsK.push_back(dom->currentTemperature().toKelvin());
    }

    pybind11::gil_scoped_acquire gil;

    pybind11::list domain_data;
    for (auto *dom : domains) {
        double power = dom->getAccumulatedTotalPower();
        double temp_k = dom->currentTemperature().toKelvin();
        domain_data.append(
            pybind11::make_tuple(dom->traceLabel(), power, temp_k));
    }

    pybind11::object result = solver->solve(domain_data, dt_s);
    lastStepSolverSubsteps = solver->getLastSolverSubsteps();

    if (pybind11::isinstance<pybind11::dict>(result)) {
        pybind11::dict temp_map = result.cast<pybind11::dict>();
        for (auto *dom : domains) {
            pybind11::str key(dom->traceLabel());
            if (!temp_map.contains(key)) {
                throw std::runtime_error(
                    "Thermal solver dict result missing key for domain: " +
                    dom->traceLabel());
            }
            double new_temp_k = temp_map[key].cast<double>();
            dom->getNode()->temp = Temperature::fromKelvin(new_temp_k);
        }
    } else {
        pybind11::sequence new_temps = result.cast<pybind11::sequence>();
        if (pybind11::len(new_temps) != domains.size()) {
            throw std::runtime_error(
                "Thermal solver returned sequence of wrong length");
        }
        for (size_t i = 0; i < domains.size(); i++) {
            double new_temp_k = new_temps[i].cast<double>();
            domains[i]->getNode()->temp = Temperature::fromKelvin(new_temp_k);
        }
    }

    lastConsumedPowerTickValue = accum_end_tick;
    hasConsumedPowerSample = true;

    traceAccumDynW.clear();
    traceAccumStW.clear();
    traceAccumTotalW.clear();
    traceAccumDynW.reserve(domains.size());
    traceAccumStW.reserve(domains.size());
    traceAccumTotalW.reserve(domains.size());
    tracePowerWindowSampleCount = accum_sample_count;
    for (auto *dom : domains) {
        traceAccumDynW.push_back(dom->getAccumulatedDynamicPower());
        traceAccumStW.push_back(dom->getAccumulatedStaticPower());
        traceAccumTotalW.push_back(dom->getAccumulatedTotalPower());
    }

    for (auto *dom : domains) {
        dom->recordTemperatureSample();
        dom->emitUpdate();
    }

    if (enable_trace && !trace_file.empty()) {
        appendTemperatureTrace();
    }

    for (auto *dom : domains) {
        dom->clearAccumulatedPower();
    }

    numThermalSteps++;
    if (reschedule_next) {
        numThermalIntervalSteps++;
    } else {
        numThermalFlushSteps++;
    }

    if (reschedule_next && stepping_active) {
        nextIntervalStepTick += intervalTicks;
        schedule(intervalStepEvent, nextIntervalStepTick);
    }

    return true;
}

void
ThermalModel::stepAtInterval()
{
    assert(intervalTicks > 0 && stepping_active && solver);

    if (!consumeSampledPower(true)) {
        numThermalWaitRetries++;

        if (waitStartTick == 0) {
            waitStartTick = curTick();
        }

        if (sample_wait_timeout_ticks > 0 &&
            curTick() - waitStartTick >= sample_wait_timeout_ticks) {
            panic("ThermalModel %s waited too long for aligned power samples "
                  "(waited %llu ticks, timeout %llu ticks).\n",
                  name(), curTick() - waitStartTick,
                  sample_wait_timeout_ticks);
        }

        schedule(intervalStepEvent, curTick() + 1);
        return;
    }

    waitStartTick = 0;
}

void
ThermalModel::flushStepNow()
{
    if (!solver || domains.empty()) {
        return;
    }

    if (intervalStepEvent.scheduled()) {
        deschedule(intervalStepEvent);
    }

    (void)consumeSampledPower(false);
}

void
ThermalModel::appendTemperatureTrace() const
{
    std::ofstream csv(trace_file, std::ios::app);
    if (!csv.is_open())
        return;

    if (csv.tellp() == 0) {
        csv << "tick,consumed_power_tick,power_window_duration_ticks,"
               "power_window_sample_count,label,dynamic_power_w,"
               "static_power_w,total_power_w,temp_k,dt_s,solver_substeps,"
               "step_kind\n";
    }

    csv << std::setprecision(17);

    for (size_t i = 0; i < domains.size(); i++) {
        auto *dom = domains[i];
        double dyn_w = i < traceAccumDynW.size() ? traceAccumDynW[i] : 0.0;
        double st_w = i < traceAccumStW.size() ? traceAccumStW[i] : 0.0;
        double total_w =
            i < traceAccumTotalW.size() ? traceAccumTotalW[i] : 0.0;
        csv << curTick() << "," << lastConsumedPowerTickValue << ","
            << lastStepDtTicksValue << "," << tracePowerWindowSampleCount
            << "," << dom->traceLabel() << "," << dyn_w << "," << st_w << ","
            << total_w << "," << dom->currentTemperature().toKelvin() << ","
            << lastStepDtSecondsValue << "," << lastStepSolverSubsteps << ","
            << lastStepKind << "\n";
    }
}

} // namespace gem5
