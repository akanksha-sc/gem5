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

#ifndef __HWMODEL_SALAM_POWER_MODEL_HH__
#define __HWMODEL_SALAM_POWER_MODEL_HH__

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "base/statistics.hh"
#include "params/SALAMPowerModel.hh"
#include "salam/LLVMRead/value.hh"
#include "sim/sim_object.hh"

using namespace gem5;

enum class SalamPowerComponent : unsigned
{
    FuDynamic = 0,
    FuStatic,
    RegDynamic,
    RegStatic,
    SpmReadDynamic,
    SpmWriteDynamic,
    SpmStatic,
    NumComponents
};

struct FUCounts
{
    int32_t counter_units = 0;
    int32_t int_adder_units = 0;
    int32_t int_multiply_units = 0;
    int32_t int_shifter_units = 0;
    int32_t int_bit_units = 0;
    int32_t fp_sp_adder = 0;
    int32_t fp_dp_adder = 0;
    int32_t fp_sp_multiply = 0;
    int32_t fp_dp_multiply = 0;
    int32_t compare = 0;
    int32_t gep = 0;
    int32_t conversion = 0;
    int32_t other = 0;
    int32_t fpDivision = 0;
};

struct RegUsage
{
    uint64_t reads = 0;
    uint64_t writes = 0;
};

class FunctionalUnitBase;
class FunctionalUnits;
class SALAMPowerModel;

class RegisterStats
{
  public:
    int reg_total = 0;
    int reg_max_usage = 0;
    int reg_avg_usage_sum = 0;
    int reg_avg_size_sum = 0;
    int cycles_tracked = 0;

    void collect(std::vector<std::shared_ptr<SALAM::Value>> &values);
    void beginCycle();
    void endCycle();
    void endCycle(SALAMPowerModel *pm, FunctionalUnits *fu);

    double averageUsage() const;
    double averageSize() const;
    RegUsage totalAccess() const;

  private:
    std::vector<SALAM::Register *> registers;
    std::vector<uint64_t> snap_reads;
    std::vector<uint64_t> snap_writes;
};

enum class HalfAdderDynamicMode : uint8_t
{
    RUNTIME = 0,
    STATIC_FLOOR = 1,
};

enum class IntMulDynamicMode : uint8_t
{
    RUNTIME = 0,
    SHARED_MACRO = 1,
    STATIC_PER_UNIT = 2,
};

enum class FpAddDynamicMode : uint8_t
{
    PER_UNIT_PIPELINE = 0,
    SHARED_MACRO = 1,
};

enum class FpMulDynamicMode : uint8_t
{
    PER_UNIT = 0,
    SHARED_MACRO = 1,
    STATIC_RTL_BUNDLE = 2,
    PER_UNIT_SERIALIZED = 3,
};

struct PowerModelConfig
{
    HalfAdderDynamicMode half_adder = HalfAdderDynamicMode::RUNTIME;
    IntMulDynamicMode int_mul = IntMulDynamicMode::RUNTIME;
    FpAddDynamicMode fp_add = FpAddDynamicMode::PER_UNIT_PIPELINE;
    FpMulDynamicMode fp_mul = FpMulDynamicMode::SHARED_MACRO;
    double dynamic_activity_scale = 1.0;
    bool static_synthesis_floor = false;
};

class PowerAccumulator
{
  public:
    double fu_leakage_per_cycle = 0;
    double fu_dynamic_energy = 0;
    double fu_final_leakage = 0;
    double fu_area = 0;
    double reg_leakage = 0;
    double reg_dynamic_energy = 0;
    double reg_area = 0;

    uint64_t half_adder_op_cycles = 0;
    uint64_t full_adder_op_cycles = 0;
    uint64_t int_multiplier_op_cycles = 0;
    uint64_t fp_sp_multiplier_op_cycles = 0;
    uint64_t fp_dp_multiplier_op_cycles = 0;

    explicit PowerAccumulator(FunctionalUnits *functional_units);

    void
    setConfig(const PowerModelConfig &cfg)
    {
        config_ = cfg;
    }
    void updateCycle(const FUCounts &units);
    void configureStaticCounts(const FUCounts &static_units);
    void prepareStaticLeakage();
    double
    lastCycleFuDynamic() const
    {
        return last_cycle_fu_dynamic_;
    }
    void finalize(const FUCounts &static_units, int cycles,
                  int half_adder_cap = 0);
    void calculateRegisterPower(const RegUsage &usage, int cycles,
                                double avg_regs, double avg_bits);
    FUCounts applyHardwareLimits(const FUCounts &units) const;

  private:
    FunctionalUnits *fu;
    PowerModelConfig config_;
    int half_adder_cap_cfg = 0;
    FUCounts static_synthesis_;
    double last_cycle_fu_dynamic_ = 0.0;
    static constexpr int kFpMacroFactor = 5;
    static constexpr int kIntMulStaticPerUnitThreshold = 6;

    bool integerOnlyStatic() const;
    int fpPipelineFactor() const;

    FunctionalUnitBase *adder() const;
    FunctionalUnitBase *multiplier() const;
    FunctionalUnitBase *bitwise() const;
    FunctionalUnitBase *shifter() const;
    FunctionalUnitBase *fpAddSp() const;
    FunctionalUnitBase *fpAddDp() const;
    FunctionalUnitBase *fpMulSp() const;
    FunctionalUnitBase *fpMulDp() const;
    FunctionalUnitBase *regBit() const;

    static constexpr double kAdd05nsInternal = 9.364555e-02;
    static constexpr double kAdd05nsSwitch = 1.900256e-01;
    static constexpr double kAdd05nsLeakage = 3.265969e-03;
    static constexpr double kAdd05nsArea = 3.793488e+02;

    void accumulateDynamic(const FUCounts &units);
    void accumulatePerCycleLeakage(const FUCounts &units);
    void calculateStaticLeakage(const FUCounts &units);
    void calculateStaticArea(const FUCounts &units);

    static FUCounts capHalfAdderUnits(const FUCounts &units, int cap);
};

class SALAMPowerModel : public SimObject
{
  public:
    struct PowerStats : public statistics::Group
    {
        statistics::Vector componentEnergy;
        statistics::Scalar accCycles;

        PowerStats(statistics::Group *parent);
    };

  private:
    PowerAccumulator *accum_;
    FunctionalUnits *functional_units_;
    PowerStats powerStats;

    uint32_t half_adder_area_cap_;
    uint8_t half_adder_dynamic_;
    uint8_t integer_mul_dynamic_;
    uint8_t fp_add_dynamic_;
    uint8_t fp_mul_dynamic_;
    double dynamic_activity_scale_;
    bool static_synthesis_floor_;

    double dynamic_power_w_ = 0.0;
    double static_power_w_ = 0.0;
    double area_um2_ = 0.0;

    bool static_fu_leakage_ready_ = false;
    bool spm_configured_ = false;
    double spm_per_access_read_mw_ = 0.0;
    double spm_per_access_write_mw_ = 0.0;
    double spm_leakage_mw_ = 0.0;
    std::array<double, static_cast<size_t>(SalamPowerComponent::NumComponents)>
        component_energy_totals_{};
    uint64_t acc_cycles_tracked_ = 0;

    void ensureAccumulator();
    void addComponentEnergy(SalamPowerComponent component, double mw_cycles);
    void ensureComponentTotal(SalamPowerComponent component,
                              double target_mw_cycles);

  public:
    SALAMPowerModel(const SALAMPowerModelParams &params);
    ~SALAMPowerModel() override;

    void bindFunctionalUnits(FunctionalUnits *fu);
    void initializePowerModel(const FUCounts &static_counts);
    void configureSpm(int spm_bytes, int read_ports, int write_ports);
    void updateCycle(const FUCounts &units);
    void noteRegisterAccess(uint64_t read_delta, uint64_t write_delta);
    void noteSpmRead();
    void noteSpmWrite();
    void finalize(const FUCounts &static_units, int cycles);
    void calculateRegisterPower(const RegUsage &usage, int cycles,
                                double avg_regs, double avg_bits);
    void syncFinalizedComponentStats(int cycles, double fu_dynamic_mw,
                                     double fu_static_mw,
                                     double reg_dynamic_mw,
                                     double reg_static_mw);
    void recordFinalizedPower(double dynamic_mw, double static_mw,
                              double area_um2);
    void regStats() override;

    const PowerAccumulator &accumulator() const;
    PowerAccumulator &accumulator();

    double
    getDynamicPower() const
    {
        return dynamic_power_w_;
    }
    double
    getStaticPower() const
    {
        return static_power_w_;
    }
    double
    getArea() const
    {
        return area_um2_;
    }

    void
    setHalfAdderAreaCap(uint32_t cap)
    {
        half_adder_area_cap_ = cap;
    }

    double
    spmPerAccessReadMw() const
    {
        return spm_per_access_read_mw_;
    }

    double
    spmPerAccessWriteMw() const
    {
        return spm_per_access_write_mw_;
    }

    double
    spmLeakageMwPerCycle() const
    {
        return spm_leakage_mw_;
    }
};

#endif // __HWMODEL_SALAM_POWER_MODEL_HH__
