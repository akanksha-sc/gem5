/*
 * Copyright (c) 2025 Akanksha Chaudhari, Matt Sinclair
 * All rights reserved.
 *
 * This file contains modifications and/or code derived from:
 * gem5-SALAM: https://github.com/TeCSAR-UNCC/gem5-SALAM
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "salam/HWModeling/salam_power_model.hh"

#include <algorithm>

#include "salam/HWModeling/cacti_wrapper.hh"
#include "salam/HWModeling/functional_units.hh"
#include "salam/HWModeling/functional_units/base.hh"

SALAMPowerModel::SALAMPowerModel(const SALAMPowerModelParams &params)
    : SimObject(params),
      accum_(nullptr),
      functional_units_(nullptr),
      powerStats(this),
      half_adder_area_cap_(params.half_adder_area_cap),
      half_adder_dynamic_(params.half_adder_dynamic),
      integer_mul_dynamic_(params.integer_mul_dynamic),
      fp_add_dynamic_(params.fp_add_dynamic),
      fp_mul_dynamic_(params.fp_mul_dynamic),
      dynamic_activity_scale_(params.dynamic_activity_scale),
      static_synthesis_floor_(params.static_synthesis_floor)
{}

SALAMPowerModel::PowerStats::PowerStats(statistics::Group *parent)
    : statistics::Group(parent, "power"),
      ADD_STAT(componentEnergy, statistics::units::Count::get(),
               "Per-component energy accumulator (mW·cycles)"),
      ADD_STAT(accCycles, statistics::units::Cycle::get(),
               "Accelerator power-model cycles tracked"),
      ADD_STAT(fuAreaUm2, statistics::units::Count::get(),
               "Functional-unit block area (um^2)"),
      ADD_STAT(regAreaUm2, statistics::units::Count::get(),
               "Register block area (um^2)"),
      ADD_STAT(spmAreaUm2, statistics::units::Count::get(),
               "SPM block area (um^2)"),
      ADD_STAT(areasReady, statistics::units::Count::get(),
               "1 once fu/reg/spm block areas are published")
{
    componentEnergy.init(static_cast<int>(SalamPowerComponent::NumComponents))
        .subname(static_cast<int>(SalamPowerComponent::FuDynamic), "fuDynamic")
        .subname(static_cast<int>(SalamPowerComponent::FuStatic), "fuStatic")
        .subname(static_cast<int>(SalamPowerComponent::RegDynamic),
                 "regDynamic")
        .subname(static_cast<int>(SalamPowerComponent::RegStatic), "regStatic")
        .subname(static_cast<int>(SalamPowerComponent::SpmReadDynamic),
                 "spmReadDynamic")
        .subname(static_cast<int>(SalamPowerComponent::SpmWriteDynamic),
                 "spmWriteDynamic")
        .subname(static_cast<int>(SalamPowerComponent::SpmStatic),
                 "spmStatic");
}

void
SALAMPowerModel::regStats()
{
    SimObject::regStats();
}

void
SALAMPowerModel::addComponentEnergy(SalamPowerComponent component,
                                    double mw_cycles)
{
    if (mw_cycles <= 0.0)
        return;
    const auto idx = static_cast<size_t>(component);
    component_energy_totals_[idx] += mw_cycles;
    powerStats.componentEnergy[static_cast<int>(component)] += mw_cycles;
}

void
SALAMPowerModel::ensureComponentTotal(SalamPowerComponent component,
                                      double target_mw_cycles)
{
    const auto idx = static_cast<size_t>(component);
    const double current = component_energy_totals_[idx];
    if (target_mw_cycles > current) {
        const double delta = target_mw_cycles - current;
        component_energy_totals_[idx] = target_mw_cycles;
        powerStats.componentEnergy[static_cast<int>(component)] += delta;
    }
}

SALAMPowerModel::~SALAMPowerModel()
{
    delete accum_;
}

namespace
{

PowerModelConfig
makePowerConfig(uint8_t half_adder_dynamic, uint8_t integer_mul_dynamic,
                uint8_t fp_add_dynamic, uint8_t fp_mul_dynamic,
                double dynamic_activity_scale, bool static_synthesis_floor)
{
    PowerModelConfig cfg;
    cfg.half_adder = static_cast<HalfAdderDynamicMode>(half_adder_dynamic);
    cfg.int_mul = static_cast<IntMulDynamicMode>(integer_mul_dynamic);
    cfg.fp_add = static_cast<FpAddDynamicMode>(fp_add_dynamic);
    cfg.fp_mul = static_cast<FpMulDynamicMode>(fp_mul_dynamic);
    cfg.dynamic_activity_scale = dynamic_activity_scale;
    cfg.static_synthesis_floor = static_synthesis_floor;
    return cfg;
}

} // namespace

const PowerAccumulator &
SALAMPowerModel::accumulator() const
{
    return *accum_;
}

PowerAccumulator &
SALAMPowerModel::accumulator()
{
    return *accum_;
}

void
SALAMPowerModel::ensureAccumulator()
{
    if (!accum_) {
        assert(functional_units_);
        accum_ = new PowerAccumulator(functional_units_);
    }
    accum_->setConfig(makePowerConfig(
        half_adder_dynamic_, integer_mul_dynamic_, fp_add_dynamic_,
        fp_mul_dynamic_, dynamic_activity_scale_, static_synthesis_floor_));
}

void
SALAMPowerModel::bindFunctionalUnits(FunctionalUnits *fu)
{
    functional_units_ = fu;
    if (accum_ && functional_units_ != fu) {
        delete accum_;
        accum_ = nullptr;
    }
}

void
SALAMPowerModel::initializePowerModel(const FUCounts &static_counts)
{
    ensureAccumulator();
    accum_->configureStaticCounts(static_counts);
    accum_->prepareStaticLeakage();
    accum_->publishStaticFuArea(static_counts, half_adder_area_cap_);
    static_fu_leakage_ready_ = true;
    updateBlockAreaStats();
}

void
SALAMPowerModel::publishStaticRegisterArea(int reg_total, int bit_width)
{
    ensureAccumulator();
    accum_->publishStaticRegisterArea(reg_total, bit_width);
    updateBlockAreaStats();
}

void
SALAMPowerModel::configureSpm(int spm_bytes, int read_ports, int write_ports)
{
    const int size = spm_bytes > 0 ? spm_bytes : 4096;
    const SpmPerAccessPower per_access =
        computeSpmPerAccess(size, read_ports, write_ports);
    const SpmAreaInfo spm_area = computeSpmArea(size, read_ports, write_ports);

    spm_per_access_read_mw_ = per_access.read_dynamic_mw;
    spm_per_access_write_mw_ = per_access.write_dynamic_mw;
    spm_leakage_mw_ = per_access.leakage_mw;
    spm_area_um2_ = spm_area.area_um2;
    spm_configured_ = true;
    updateBlockAreaStats();
}

void
SALAMPowerModel::updateBlockAreaStats()
{
    const double fu_area = accum_ ? accum_->fu_area : 0.0;
    const double reg_area = accum_ ? accum_->reg_area : 0.0;

    powerStats.fuAreaUm2 = fu_area;
    powerStats.regAreaUm2 = reg_area;
    powerStats.spmAreaUm2 = spm_area_um2_;

    const bool ready = fu_area > 0.0 && reg_area > 0.0 && spm_area_um2_ > 0.0;
    powerStats.areasReady = ready ? 1.0 : 0.0;
}

void
SALAMPowerModel::updateCycle(const FUCounts &units)
{
    if (!accum_)
        return;

    const double before = accum_->fu_dynamic_energy;
    accum_->updateCycle(units);
    addComponentEnergy(SalamPowerComponent::FuDynamic,
                       accum_->fu_dynamic_energy - before);

    if (static_fu_leakage_ready_)
        addComponentEnergy(SalamPowerComponent::FuStatic,
                           accum_->fu_final_leakage);

    if (spm_configured_)
        addComponentEnergy(SalamPowerComponent::SpmStatic, spm_leakage_mw_);

    powerStats.accCycles++;
    acc_cycles_tracked_++;
}

void
SALAMPowerModel::noteRegisterAccess(uint64_t read_delta, uint64_t write_delta)
{
    if (!functional_units_ || (read_delta == 0 && write_delta == 0))
        return;

    FunctionalUnitBase *reg = functional_units_->_bit_register;
    const double reg_dyn = reg->get_internal_power() + reg->get_switch_power();
    addComponentEnergy(SalamPowerComponent::RegDynamic,
                       static_cast<double>(read_delta + write_delta) *
                           reg_dyn);
}

void
SALAMPowerModel::noteSpmRead()
{
    if (!spm_configured_)
        return;
    addComponentEnergy(SalamPowerComponent::SpmReadDynamic,
                       spm_per_access_read_mw_);
}

void
SALAMPowerModel::noteSpmWrite()
{
    if (!spm_configured_)
        return;
    addComponentEnergy(SalamPowerComponent::SpmWriteDynamic,
                       spm_per_access_write_mw_);
}

void
SALAMPowerModel::syncFinalizedComponentStats(int cycles, double fu_dynamic_mw,
                                             double fu_static_mw,
                                             double reg_dynamic_mw,
                                             double reg_static_mw)
{
    if (cycles <= 0)
        return;

    ensureComponentTotal(SalamPowerComponent::FuDynamic,
                         fu_dynamic_mw * cycles);
    ensureComponentTotal(SalamPowerComponent::FuStatic, fu_static_mw * cycles);
    ensureComponentTotal(SalamPowerComponent::RegDynamic,
                         reg_dynamic_mw * cycles);
    ensureComponentTotal(SalamPowerComponent::RegStatic,
                         reg_static_mw * cycles);

    const double target_cycles = static_cast<double>(cycles);
    if (target_cycles > static_cast<double>(acc_cycles_tracked_)) {
        const double delta = target_cycles - acc_cycles_tracked_;
        acc_cycles_tracked_ = cycles;
        powerStats.accCycles += delta;
    }
}

void
SALAMPowerModel::finalize(const FUCounts &static_units, int cycles)
{
    if (!accum_)
        return;
    accum_->finalize(static_units, cycles, half_adder_area_cap_);
}

void
SALAMPowerModel::calculateRegisterPower(const RegUsage &usage, int cycles,
                                        double avg_regs, double avg_bits)
{
    if (!accum_)
        return;
    accum_->calculateRegisterPower(usage, cycles, avg_regs, avg_bits);
}

void
SALAMPowerModel::recordFinalizedPower(double dynamic_mw, double static_mw,
                                      double area_um2)
{
    dynamic_power_w_ = dynamic_mw * 1e-3;
    static_power_w_ = static_mw * 1e-3;
    area_um2_ = area_um2;
}

void
RegisterStats::collect(std::vector<std::shared_ptr<SALAM::Value>> &values)
{
    registers.clear();
    for (auto &val : values) {
        if (!val->isInstruction())
            continue;
        auto reg = val->getReg();
        if (reg && reg->isTracked())
            registers.push_back(reg.get());
    }
    reg_total = registers.size();
}

void
RegisterStats::beginCycle()
{
    snap_reads.clear();
    snap_writes.clear();
    for (auto *reg : registers) {
        snap_reads.push_back(reg->getReads());
        snap_writes.push_back(reg->getWrites());
    }
}

void
RegisterStats::endCycle(SALAMPowerModel *pm, FunctionalUnits *fu)
{
    if (snap_reads.size() != registers.size())
        return;

    int count = 0;
    int size = 0;
    uint64_t read_delta = 0;
    uint64_t write_delta = 0;
    for (size_t i = 0; i < registers.size(); ++i) {
        auto *reg = registers[i];
        const uint64_t reads = reg->getReads();
        const uint64_t writes = reg->getWrites();
        read_delta += reads - snap_reads[i];
        write_delta += writes - snap_writes[i];
        if (reads > snap_reads[i] || writes > snap_writes[i]) {
            count++;
            size += 32;
        }
    }
    reg_avg_usage_sum += count;
    reg_avg_size_sum += size;
    if (count > reg_max_usage)
        reg_max_usage = count;
    cycles_tracked++;

    if (pm && fu)
        pm->noteRegisterAccess(read_delta, write_delta);
}

void
RegisterStats::endCycle()
{
    endCycle(nullptr, nullptr);
}

double
RegisterStats::averageUsage() const
{
    if (cycles_tracked == 0)
        return 0;
    return (double)reg_avg_usage_sum / cycles_tracked;
}

double
RegisterStats::averageSize() const
{
    if (reg_avg_usage_sum == 0)
        return 0;
    return (double)reg_avg_size_sum / reg_avg_usage_sum;
}

RegUsage
RegisterStats::totalAccess() const
{
    RegUsage usage;
    for (auto *reg : registers) {
        usage.reads += reg->getReads();
        usage.writes += reg->getWrites();
    }
    return usage;
}

PowerAccumulator::PowerAccumulator(FunctionalUnits *functional_units)
    : fu(functional_units)
{}

FunctionalUnitBase *
PowerAccumulator::adder() const
{
    return fu->_integer_adder;
}

FunctionalUnitBase *
PowerAccumulator::multiplier() const
{
    return fu->_integer_multiplier;
}

FunctionalUnitBase *
PowerAccumulator::bitwise() const
{
    return fu->_bitwise_operations;
}

FunctionalUnitBase *
PowerAccumulator::shifter() const
{
    return fu->_bit_shifter;
}

FunctionalUnitBase *
PowerAccumulator::fpAddSp() const
{
    return fu->_float_adder;
}

FunctionalUnitBase *
PowerAccumulator::fpAddDp() const
{
    return fu->_double_adder;
}

FunctionalUnitBase *
PowerAccumulator::fpMulSp() const
{
    return fu->_float_multiplier;
}

FunctionalUnitBase *
PowerAccumulator::fpMulDp() const
{
    return fu->_double_multiplier;
}

FunctionalUnitBase *
PowerAccumulator::regBit() const
{
    return fu->_bit_register;
}

int
PowerAccumulator::fpPipelineFactor() const
{
    const int cycles = static_cast<int>(fpMulDp()->get_cycles());
    return cycles > 0 ? cycles : kFpMacroFactor;
}

static int32_t
capFuCount(int32_t count, uint32_t limit)
{
    if (limit == 0)
        return count;
    return std::min(count, static_cast<int32_t>(limit));
}

FUCounts
PowerAccumulator::capHalfAdderUnits(const FUCounts &units, int cap)
{
    if (cap <= 0)
        return units;

    const int total = units.compare + units.gep;
    if (total <= cap)
        return units;

    FUCounts capped = units;
    capped.compare = static_cast<int32_t>(
        (static_cast<int64_t>(units.compare) * cap + total / 2) / total);
    capped.gep = cap - capped.compare;
    return capped;
}

FUCounts
PowerAccumulator::applyHardwareLimits(const FUCounts &units) const
{
    FUCounts capped = units;
    capped.int_adder_units =
        capFuCount(units.int_adder_units, adder()->get_limit());
    capped.int_multiply_units =
        capFuCount(units.int_multiply_units, multiplier()->get_limit());
    capped.int_shifter_units =
        capFuCount(units.int_shifter_units, shifter()->get_limit());
    capped.int_bit_units =
        capFuCount(units.int_bit_units, bitwise()->get_limit());
    capped.fp_sp_adder = capFuCount(units.fp_sp_adder, fpAddSp()->get_limit());
    capped.fp_dp_adder = capFuCount(units.fp_dp_adder, fpAddDp()->get_limit());
    capped.fp_sp_multiply =
        capFuCount(units.fp_sp_multiply, fpMulSp()->get_limit());
    capped.fp_dp_multiply =
        capFuCount(units.fp_dp_multiply, fpMulDp()->get_limit());
    return capped;
}

void
PowerAccumulator::accumulatePerCycleLeakage(const FUCounts &units)
{
    fu_leakage_per_cycle +=
        adder()->get_leakage_power() * units.int_adder_units;
    fu_leakage_per_cycle +=
        multiplier()->get_leakage_power() * units.int_multiply_units;
    fu_leakage_per_cycle +=
        bitwise()->get_leakage_power() * units.int_bit_units;
    fu_leakage_per_cycle +=
        shifter()->get_leakage_power() * units.int_shifter_units;
    fu_leakage_per_cycle += fpAddSp()->get_leakage_power() * units.fp_sp_adder;
    fu_leakage_per_cycle += fpAddDp()->get_leakage_power() * units.fp_dp_adder;
    fu_leakage_per_cycle +=
        fpMulSp()->get_leakage_power() * units.fp_sp_multiply;
    fu_leakage_per_cycle +=
        fpMulDp()->get_leakage_power() * units.fp_dp_multiply;
}

bool
PowerAccumulator::integerOnlyStatic() const
{
    return static_synthesis_.fp_dp_adder == 0 &&
           static_synthesis_.fp_sp_adder == 0 &&
           static_synthesis_.fp_dp_multiply == 0 &&
           static_synthesis_.fp_sp_multiply == 0;
}

void
PowerAccumulator::configureStaticCounts(const FUCounts &static_units)
{
    static_synthesis_ = static_units;
}

void
PowerAccumulator::prepareStaticLeakage()
{
    const FUCounts synthesis = applyHardwareLimits(static_synthesis_);
    calculateStaticLeakage(synthesis);
}

void
PowerAccumulator::accumulateDynamic(const FUCounts &units)
{
    double cycle_dynamic = 0;
    double static_rtl_bundle = 0;
    const double add_dyn =
        adder()->get_switch_power() + adder()->get_internal_power();
    const double mul_dyn =
        multiplier()->get_switch_power() + multiplier()->get_internal_power();
    const double fp_sp_add_dyn =
        fpAddSp()->get_switch_power() + fpAddSp()->get_internal_power();
    const double fp_dp_add_dyn =
        fpAddDp()->get_switch_power() + fpAddDp()->get_internal_power();
    const double fp_sp_mul_dyn =
        fpMulSp()->get_switch_power() + fpMulSp()->get_internal_power();
    const double fp_dp_mul_dyn =
        fpMulDp()->get_switch_power() + fpMulDp()->get_internal_power();
    const int pipe = fpPipelineFactor();
    const double half_dyn = kAdd05nsInternal + kAdd05nsSwitch;

    if (config_.static_synthesis_floor) {
        const int s_half = static_synthesis_.compare + static_synthesis_.gep;
        cycle_dynamic += static_synthesis_.int_adder_units * add_dyn;
        cycle_dynamic += static_synthesis_.int_multiply_units * mul_dyn;
        cycle_dynamic += s_half * half_dyn;
        cycle_dynamic += static_synthesis_.fp_dp_adder * fp_dp_add_dyn * pipe;
        if (static_synthesis_.fp_dp_multiply > 0)
            cycle_dynamic += fp_dp_mul_dyn * pipe;
        else if (static_synthesis_.fp_sp_multiply > 0)
            cycle_dynamic += fp_sp_mul_dyn * pipe;
        full_adder_op_cycles += static_synthesis_.int_adder_units;
        int_multiplier_op_cycles += static_synthesis_.int_multiply_units;
        half_adder_op_cycles += s_half;
        if (static_synthesis_.fp_dp_multiply > 0)
            fp_dp_multiplier_op_cycles += static_synthesis_.fp_dp_multiply;
        const double scaled = cycle_dynamic * config_.dynamic_activity_scale;
        fu_dynamic_energy += scaled;
        return;
    }

    cycle_dynamic += add_dyn * units.int_adder_units;
    full_adder_op_cycles += units.int_adder_units;

    switch (config_.int_mul) {
        case IntMulDynamicMode::STATIC_PER_UNIT:
            if (static_synthesis_.int_multiply_units > 0)
                cycle_dynamic +=
                    static_synthesis_.int_multiply_units * mul_dyn * pipe;
            int_multiplier_op_cycles += static_synthesis_.int_multiply_units;
            break;
        case IntMulDynamicMode::SHARED_MACRO:
            if (units.int_multiply_units > 0)
                cycle_dynamic += mul_dyn * pipe;
            int_multiplier_op_cycles += units.int_multiply_units;
            break;
        case IntMulDynamicMode::RUNTIME:
        default: {
            const int static_int_mul = static_synthesis_.int_multiply_units;
            if (static_int_mul > 0 && integerOnlyStatic()) {
                if (static_int_mul >= kIntMulStaticPerUnitThreshold)
                    cycle_dynamic += static_int_mul * mul_dyn * pipe;
                else
                    cycle_dynamic += mul_dyn * pipe;
            } else {
                cycle_dynamic += mul_dyn * units.int_multiply_units;
            }
            int_multiplier_op_cycles += units.int_multiply_units;
            break;
        }
    }

    cycle_dynamic +=
        (bitwise()->get_switch_power() + bitwise()->get_internal_power()) *
        units.int_bit_units;
    cycle_dynamic +=
        (shifter()->get_switch_power() + shifter()->get_internal_power()) *
        units.int_shifter_units;

    const bool serialized_fp_mul =
        config_.fp_mul == FpMulDynamicMode::PER_UNIT_SERIALIZED ||
        fpMulDp()->get_limit() == 1;

    if (serialized_fp_mul) {
        if (config_.fp_add == FpAddDynamicMode::SHARED_MACRO) {
            if (units.fp_dp_adder > 0)
                cycle_dynamic += fp_dp_add_dyn * pipe;
        } else {
            cycle_dynamic += fp_dp_add_dyn * units.fp_dp_adder * pipe;
        }
        cycle_dynamic += fp_sp_add_dyn * units.fp_sp_adder * pipe;
        if (units.fp_dp_multiply > 0) {
            cycle_dynamic += fp_dp_mul_dyn * units.fp_dp_multiply;
            fp_dp_multiplier_op_cycles += units.fp_dp_multiply;
        }
        if (units.fp_sp_multiply > 0) {
            cycle_dynamic += fp_sp_mul_dyn * units.fp_sp_multiply;
            fp_sp_multiplier_op_cycles += units.fp_sp_multiply;
        }
    } else {
        if (config_.fp_add == FpAddDynamicMode::SHARED_MACRO) {
            if (units.fp_sp_adder > 0)
                cycle_dynamic += fp_sp_add_dyn * pipe;
            if (units.fp_dp_adder > 0)
                cycle_dynamic += fp_dp_add_dyn * pipe;
        } else {
            cycle_dynamic += fp_sp_add_dyn * units.fp_sp_adder * pipe;
            cycle_dynamic += fp_dp_add_dyn * units.fp_dp_adder * pipe;
        }

        if (config_.fp_mul == FpMulDynamicMode::STATIC_RTL_BUNDLE) {
            if (static_synthesis_.fp_dp_multiply > 0) {
                static_rtl_bundle += fp_dp_mul_dyn * pipe;
                fp_dp_multiplier_op_cycles += static_synthesis_.fp_dp_multiply;
            } else if (static_synthesis_.fp_sp_multiply > 0) {
                static_rtl_bundle += fp_sp_mul_dyn * pipe;
                fp_sp_multiplier_op_cycles += static_synthesis_.fp_sp_multiply;
            }
        } else if (config_.fp_mul == FpMulDynamicMode::PER_UNIT) {
            if (units.fp_sp_multiply > 0) {
                cycle_dynamic += fp_sp_mul_dyn * units.fp_sp_multiply;
                fp_sp_multiplier_op_cycles += units.fp_sp_multiply;
            }
            if (units.fp_dp_multiply > 0) {
                cycle_dynamic += fp_dp_mul_dyn * units.fp_dp_multiply;
                fp_dp_multiplier_op_cycles += units.fp_dp_multiply;
            }
        } else {
            if (units.fp_sp_multiply > 0) {
                cycle_dynamic += fp_sp_mul_dyn * pipe;
                fp_sp_multiplier_op_cycles += units.fp_sp_multiply;
            }
            if (units.fp_dp_multiply > 0) {
                cycle_dynamic += fp_dp_mul_dyn * pipe;
                fp_dp_multiplier_op_cycles += units.fp_dp_multiply;
            }
        }
    }

    if (config_.half_adder == HalfAdderDynamicMode::STATIC_FLOOR) {
        const int static_half = static_synthesis_.compare +
                                static_synthesis_.gep +
                                static_synthesis_.conversion;
        cycle_dynamic += half_dyn * static_half;
        half_adder_op_cycles += static_half;
    } else {
        const int half_adder_ops =
            units.counter_units + units.conversion + units.compare + units.gep;
        half_adder_op_cycles += half_adder_ops;
        cycle_dynamic += half_dyn * half_adder_ops;
    }

    cycle_dynamic += static_rtl_bundle;

    double scaled = cycle_dynamic;
    if (config_.dynamic_activity_scale != 1.0) {
        if (config_.fp_mul == FpMulDynamicMode::STATIC_RTL_BUNDLE &&
            static_rtl_bundle > 0.0) {
            scaled = static_rtl_bundle + (cycle_dynamic - static_rtl_bundle) *
                                             config_.dynamic_activity_scale;
        } else {
            scaled = cycle_dynamic * config_.dynamic_activity_scale;
        }
    }

    fu_dynamic_energy += scaled;
}

void
PowerAccumulator::updateCycle(const FUCounts &units)
{
    last_cycle_fu_dynamic_ = 0.0;
    const double before = fu_dynamic_energy;
    accumulatePerCycleLeakage(units);
    accumulateDynamic(units);
    last_cycle_fu_dynamic_ = fu_dynamic_energy - before;
}

void
PowerAccumulator::calculateStaticLeakage(const FUCounts &units)
{
    fu_final_leakage = adder()->get_leakage_power() * units.int_adder_units;
    fu_final_leakage +=
        multiplier()->get_leakage_power() * units.int_multiply_units;
    fu_final_leakage += bitwise()->get_leakage_power() * units.int_bit_units;
    fu_final_leakage +=
        shifter()->get_leakage_power() * units.int_shifter_units;
    fu_final_leakage += fpAddSp()->get_leakage_power() * units.fp_sp_adder;
    fu_final_leakage += fpAddDp()->get_leakage_power() * units.fp_dp_adder;
    if (units.fp_sp_multiply > 0)
        fu_final_leakage += fpMulSp()->get_leakage_power();
    if (units.fp_dp_multiply > 0)
        fu_final_leakage += fpMulDp()->get_leakage_power();

    const int half_adder_static = units.conversion + units.compare + units.gep;
    fu_final_leakage += kAdd05nsLeakage * half_adder_static;
}

void
PowerAccumulator::calculateStaticArea(const FUCounts &units)
{
    const bool has_fp = units.fp_dp_adder > 0 || units.fp_sp_adder > 0 ||
                        units.fp_dp_multiply > 0 || units.fp_sp_multiply > 0;
    const bool integer_only = units.int_multiply_units == 0 && !has_fp;

    fu_area = multiplier()->get_area() * units.int_multiply_units;
    fu_area += bitwise()->get_area() * units.int_bit_units;
    fu_area += shifter()->get_area() * units.int_shifter_units;
    fu_area += fpAddDp()->get_area() * units.fp_dp_adder;
    fu_area += fpAddSp()->get_area() * units.fp_sp_adder;
    if (units.fp_sp_multiply > 0)
        fu_area += fpMulSp()->get_area();
    if (units.fp_dp_multiply > 0)
        fu_area += fpMulDp()->get_area();

    if (units.int_adder_units > 0 &&
        (integer_only ||
         (units.int_multiply_units > 0 && half_adder_cap_cfg > 0)))
        fu_area += adder()->get_area() * units.int_adder_units;

    const int half_adder_static = units.compare + units.gep;
    if (half_adder_static > 0 &&
        (integer_only ||
         (units.int_multiply_units > 0 && half_adder_cap_cfg > 0)))
        fu_area += kAdd05nsArea * half_adder_static;
}

void
PowerAccumulator::calculateRegisterPower(const RegUsage &usage, int cycles,
                                         double avg_regs, double avg_bits)
{
    if (cycles <= 0 || avg_regs <= 0)
        return;

    const double scale = (avg_regs / cycles) * (avg_bits / avg_regs);
    const double reg_dyn =
        regBit()->get_internal_power() + regBit()->get_switch_power();
    reg_leakage = regBit()->get_leakage_power() * scale;
    reg_dynamic_energy =
        ((double)usage.reads + (double)usage.writes) * scale * reg_dyn;
    reg_area = scale * regBit()->get_area();
}

void
PowerAccumulator::publishStaticFuArea(const FUCounts &static_units,
                                      int half_adder_cap)
{
    half_adder_cap_cfg = half_adder_cap;
    const FUCounts synthesis = applyHardwareLimits(static_units);
    const FUCounts area_units = capHalfAdderUnits(synthesis, half_adder_cap);
    calculateStaticArea(area_units);
}

void
PowerAccumulator::publishStaticRegisterArea(int reg_total, int bit_width)
{
    if (reg_total <= 0 || bit_width <= 0) {
        reg_area = regBit()->get_area() * 32.0;
        return;
    }

    reg_area = reg_total * bit_width * regBit()->get_area();
}

void
PowerAccumulator::finalize(const FUCounts &static_units, int cycles,
                           int half_adder_cap)
{
    half_adder_cap_cfg = half_adder_cap;
    const FUCounts synthesis = applyHardwareLimits(static_units);
    calculateStaticLeakage(synthesis);
    FUCounts area_units = capHalfAdderUnits(synthesis, half_adder_cap);
    calculateStaticArea(area_units);
}
