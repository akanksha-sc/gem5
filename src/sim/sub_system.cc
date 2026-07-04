/*
 * Copyright (c) 2014-2016 ARM Limited
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

#include "sim/sub_system.hh"

#include "params/SubSystem.hh"
#include "sim/power/power_model.hh"
#include "sim/power/thermal_domain.hh"

namespace gem5
{

SubSystem::SubSystem(const Params &p)
 : SimObject(p)
{
    // Link thermalDomain <-> SubSystem
    if (p.thermal_domain)
        p.thermal_domain->setSubSystem(this);
}

double
SubSystem::getDynamicPower() const
{
    double ret = 0.0f;
    for (auto &obj: powerProducers)
        ret += obj->getDynamicPower();
    return ret;
}

double
SubSystem::getStaticPower() const
{
    double ret = 0.0f;
    for (auto &obj: powerProducers)
        ret += obj->getStaticPower();
    return ret;
}

double
SubSystem::getSampledDynamicPower() const
{
    double ret = 0.0f;
    for (auto &obj : powerProducers)
        ret += obj->getSampledDynamicPower();
    return ret;
}

double
SubSystem::getSampledStaticPower() const
{
    double ret = 0.0f;
    for (auto &obj : powerProducers)
        ret += obj->getSampledStaticPower();
    return ret;
}

double
SubSystem::getSampledTotalPower() const
{
    return getSampledDynamicPower() + getSampledStaticPower();
}

Tick
SubSystem::getSampledPowerTick() const
{
    Tick common = 0;

    for (auto &obj : powerProducers) {
        Tick t = obj->getSampledPowerTick();

        if (t == 0) {
            return 0;
        }

        if (common == 0) {
            common = t;
        } else if (t != common) {
            return 0;
        }
    }

    return common;
}

namespace
{

Tick
commonProducerTick(const std::vector<PowerModel *> &producers,
                   Tick (PowerModel::*getter)() const)
{
    Tick common = 0;
    for (auto *obj : producers) {
        Tick t = (obj->*getter)();
        if (t == 0)
            return 0;
        if (common == 0)
            common = t;
        else if (common != t)
            return 0;
    }
    return common;
}

uint64_t
commonProducerSampleCount(const std::vector<PowerModel *> &producers)
{
    uint64_t common = 0;
    for (auto *obj : producers) {
        uint64_t c = obj->getAccumulatedPowerSampleCount();
        if (c == 0)
            return 0;
        if (common == 0)
            common = c;
        else if (common != c)
            return 0;
    }
    return common;
}

} // anonymous namespace

double
SubSystem::getAccumulatedDynamicPower() const
{
    double ret = 0.0f;
    for (auto &obj : powerProducers)
        ret += obj->getAccumulatedDynamicPower();
    return ret;
}

double
SubSystem::getAccumulatedStaticPower() const
{
    double ret = 0.0f;
    for (auto &obj : powerProducers)
        ret += obj->getAccumulatedStaticPower();
    return ret;
}

double
SubSystem::getAccumulatedTotalPower() const
{
    return getAccumulatedDynamicPower() + getAccumulatedStaticPower();
}

Tick
SubSystem::getAccumulatedPowerTick() const
{
    return commonProducerTick(powerProducers,
                              &PowerModel::getAccumulatedPowerTick);
}

Tick
SubSystem::getAccumulatedPowerDurationTicks() const
{
    return commonProducerTick(powerProducers,
                              &PowerModel::getAccumulatedPowerDurationTicks);
}

uint64_t
SubSystem::getAccumulatedPowerSampleCount() const
{
    return commonProducerSampleCount(powerProducers);
}

void
SubSystem::clearAccumulatedPower()
{
    for (auto &obj : powerProducers)
        obj->clearAccumulatedPower();
}

} // namespace gem5
