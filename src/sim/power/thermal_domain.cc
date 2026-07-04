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

#include "sim/power/thermal_domain.hh"

#include <algorithm>

#include "base/statistics.hh"
#include "debug/ThermalDomain.hh"
#include "params/ThermalDomain.hh"
#include "sim/clocked_object.hh"
#include "sim/linear_solver.hh"
#include "sim/power/power_model.hh"
#include "sim/power/thermal_model.hh"
#include "sim/probe/probe.hh"
#include "sim/sub_system.hh"

namespace gem5
{

ThermalDomain::ThermalDomain(const Params &p)
    : SimObject(p),
      _initTemperature(p.initial_temperature),
      _label(p.label),
      node(NULL),
      subsystem(NULL),
      directPowerModel(p.power_model),
      ADD_STAT(currentTemp, statistics::units::DegreeCelsius::get(),
               "Temperature"),
      ADD_STAT(sampledDynamicPower, statistics::units::Watt::get(),
               "Last sampled dynamic power consumed by this thermal domain"),
      ADD_STAT(sampledStaticPower, statistics::units::Watt::get(),
               "Last sampled static power consumed by this thermal domain"),
      ADD_STAT(sampledTotalPower, statistics::units::Watt::get(),
               "Last sampled total power consumed by this thermal domain"),
      ADD_STAT(sampledPowerTick,
               "Tick of last sampled power consumed by this thermal domain"),
      ADD_STAT(sampledTemperatureK, "Temperature (Kelvin) used for the last "
                                    "power sample in this domain"),
      ADD_STAT(tempDist, "Temperature sample distribution (Kelvin)"),
      ADD_STAT(tempSampleCount, "Number of temperature samples"),
      ADD_STAT(tempMinK, "Minimum sampled temperature (Kelvin)"),
      ADD_STAT(tempMaxK, "Maximum sampled temperature (Kelvin)"),
      ADD_STAT(tempAvgK, "Average sampled temperature (Kelvin)"),
      ADD_STAT(tempFinalK, "Final sampled temperature (Kelvin)")
{
    currentTemp
        .functor([this]() { return currentTemperature().toCelsius(); });
    sampledDynamicPower.functor([this]() { return getSampledDynamicPower(); });
    sampledStaticPower.functor([this]() { return getSampledStaticPower(); });
    sampledTotalPower.functor([this]() { return getSampledPower(); });
    sampledPowerTick.functor(
        [this]() -> double { return getSampledPowerTick(); });
    sampledTemperatureK.functor(
        [this]() -> double { return getSampledTemperatureKelvin(); });
    tempDist.init(20);
    tempSampleCount.functor([this]() -> double { return _temp_sample_count; });
    tempMinK.functor([this]() -> double {
        return _temp_sample_count > 0 ? _min_temp_k : 0.0;
    });
    tempMaxK.functor([this]() -> double { return _max_temp_k; });
    tempAvgK.functor([this]() -> double {
        return _temp_sample_count > 0 ? _sum_temp_k / _temp_sample_count : 0.0;
    });
    tempFinalK.functor([this]() -> double { return _final_temp_k; });
}

Temperature
ThermalDomain::currentTemperature() const
{
    return node->temp;
}

void
ThermalDomain::setSubSystem(SubSystem * ss)
{
    assert(!this->subsystem);
    this->subsystem = ss;

    ppThermalUpdate = new ProbePointArg<Temperature>(
        subsystem->getProbeManager(), "thermalUpdate");
}

void
ThermalDomain::emitUpdate()
{
    // Direct power-model update path: if this domain is bound directly to a
    // PowerModel, propagate the new temperature immediately.
    if (directPowerModel) {
        directPowerModel->thermalUpdateCallback(node->temp);
    }

    // Existing SubSystem/probe update path.
    if (ppThermalUpdate) {
        ppThermalUpdate->notify(node->temp);
    }
}


LinearEquation
ThermalDomain::getEquation(ThermalNode * tn, unsigned n, double step) const
{
    LinearEquation eq(n);

    double power = 0.0;
    if (subsystem) {
        power = subsystem->getDynamicPower() + subsystem->getStaticPower();
    } else if (directPowerModel) {
        power = directPowerModel->getDynamicPower() +
                directPowerModel->getStaticPower();
    } else {
        DPRINTF(ThermalDomain,
                "ThermalDomain %s has no subsystem or direct power model; "
                "using 0W power source\n",
                name());
    }

    if (tn == node)
        eq[eq.cnt()] = power;

    return eq;
}

double
ThermalDomain::getSampledDynamicPower() const
{
    if (subsystem)
        return subsystem->getSampledDynamicPower();
    if (directPowerModel)
        return directPowerModel->getSampledDynamicPower();
    return 0.0;
}

double
ThermalDomain::getSampledStaticPower() const
{
    if (subsystem)
        return subsystem->getSampledStaticPower();
    if (directPowerModel)
        return directPowerModel->getSampledStaticPower();
    return 0.0;
}

double
ThermalDomain::getSampledPower() const
{
    return getSampledDynamicPower() + getSampledStaticPower();
}

Tick
ThermalDomain::getSampledPowerTick() const
{
    if (subsystem)
        return subsystem->getSampledPowerTick();
    if (directPowerModel)
        return directPowerModel->getSampledPowerTick();
    return 0;
}

double
ThermalDomain::getSampledTemperatureKelvin() const
{
    if (directPowerModel)
        return directPowerModel->getSampledTemperatureKelvin();
    return 0.0;
}

double
ThermalDomain::getAccumulatedDynamicPower() const
{
    if (subsystem)
        return subsystem->getAccumulatedDynamicPower();
    if (directPowerModel)
        return directPowerModel->getAccumulatedDynamicPower();
    return 0.0;
}

double
ThermalDomain::getAccumulatedStaticPower() const
{
    if (subsystem)
        return subsystem->getAccumulatedStaticPower();
    if (directPowerModel)
        return directPowerModel->getAccumulatedStaticPower();
    return 0.0;
}

double
ThermalDomain::getAccumulatedTotalPower() const
{
    return getAccumulatedDynamicPower() + getAccumulatedStaticPower();
}

double
ThermalDomain::getAccumulatedPower() const
{
    return getAccumulatedTotalPower();
}

Tick
ThermalDomain::getAccumulatedPowerTick() const
{
    if (subsystem)
        return subsystem->getAccumulatedPowerTick();
    if (directPowerModel)
        return directPowerModel->getAccumulatedPowerTick();
    return 0;
}

Tick
ThermalDomain::getAccumulatedPowerDurationTicks() const
{
    if (subsystem)
        return subsystem->getAccumulatedPowerDurationTicks();
    if (directPowerModel)
        return directPowerModel->getAccumulatedPowerDurationTicks();
    return 0;
}

uint64_t
ThermalDomain::getAccumulatedPowerSampleCount() const
{
    if (subsystem)
        return subsystem->getAccumulatedPowerSampleCount();
    if (directPowerModel)
        return directPowerModel->getAccumulatedPowerSampleCount();
    return 0;
}

void
ThermalDomain::clearAccumulatedPower()
{
    if (subsystem)
        subsystem->clearAccumulatedPower();
    else if (directPowerModel)
        directPowerModel->clearAccumulatedPower();
}

void
ThermalDomain::recordTemperatureSample()
{
    double temp_k = node->temp.toKelvin();
    _temp_sample_count++;
    _sum_temp_k += temp_k;

    if (_temp_sample_count == 1 || temp_k < _min_temp_k)
        _min_temp_k = temp_k;
    if (temp_k > _max_temp_k)
        _max_temp_k = temp_k;
    _final_temp_k = temp_k;

    tempDist.sample(temp_k, 1);
}

} // namespace gem5
