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

#ifndef __HWMODEL_FUNCTIONAL_UNIT_BASE_HH__
#define __HWMODEL_FUNCTIONAL_UNIT_BASE_HH__

#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>

class FunctionalUnitBase
{
  private:
  protected:
    std::string _alias;
    uint32_t _stages;
    uint32_t _cycles;
    uint32_t _enum_value;
    std::string _int_size;
    std::string _int_sign;
    bool _int_apmode;
    std::string _fp_size;
    std::string _fp_sign;
    bool _fp_apmode;
    std::string _ptr_size;
    std::string _ptr_sign;
    bool _ptr_apmode;
    uint32_t _limit;
    std::string _power_units;
    std::string _energy_units;
    std::string _time_units;
    std::string _area_units;
    uint32_t _fu_latency;
    double _internal_power;
    double _switch_power;
    double _dynamic_power;
    double _dynamic_energy;
    double _leakage_power;
    double _area;
    double _path_delay;

    uint64_t _available = 0;

    uint64_t _in_use = 0;
    uint64_t _accept_count = 0;
    uint64_t _deny_count = 0;
    uint64_t _busy_slot_sum = 0;
    uint64_t _active_cycles = 0;
    uint64_t _peak_busy_slots = 0;

    uint64_t _cycle_accepts = 0;
    uint64_t _cycle_denies = 0;
    uint64_t _cycle_peak_in_use = 0;

  public:
    FunctionalUnitBase();
    FunctionalUnitBase(std::string alias, uint32_t stages, uint32_t cycles,
                       uint32_t enum_value, std::string int_size,
                       std::string int_sign, bool int_apmode,
                       std::string fp_size, std::string fp_sign,
                       bool fp_apmode, std::string ptr_size,
                       std::string ptr_sign, bool ptr_apmode, uint32_t limit,
                       std::string power_units, std::string energy_units,
                       std::string time_units, std::string area_units,
                       uint32_t fu_latency, double internal_power,
                       double switch_power, double dynamic_power,
                       double dynamic_energy, double leakage_power,
                       double area, double path_delay)
        : _alias(alias),
          _stages(stages),
          _cycles(cycles),
          _enum_value(enum_value),
          _int_size(int_size),
          _int_sign(int_sign),
          _int_apmode(int_apmode),
          _fp_size(fp_size),
          _fp_sign(fp_sign),
          _fp_apmode(fp_apmode),
          _ptr_size(ptr_size),
          _ptr_sign(ptr_sign),
          _ptr_apmode(ptr_apmode),
          _limit(limit),
          _power_units(power_units),
          _energy_units(energy_units),
          _time_units(time_units),
          _area_units(area_units),
          _fu_latency(fu_latency),
          _internal_power(internal_power),
          _switch_power(switch_power),
          _dynamic_power(dynamic_power),
          _dynamic_energy(dynamic_energy),
          _leakage_power(leakage_power),
          _area(area),
          _path_delay(path_delay),
          _available(0),
          _in_use(0)
    {}
    std::string
    get_alias()
    {
        return _alias;
    }
    uint32_t
    get_stages()
    {
        return _stages;
    }
    uint32_t
    get_cycles()
    {
        return _cycles;
    }
    uint32_t
    get_enum_value()
    {
        return _enum_value;
    }
    std::string
    get_int_size()
    {
        return _int_size;
    }
    std::string
    get_int_sign()
    {
        return _int_sign;
    }
    bool
    get_int_apmode()
    {
        return _int_apmode;
    }
    std::string
    get_fp_size()
    {
        return _fp_size;
    }
    std::string
    get_fp_sign()
    {
        return _fp_sign;
    }
    bool
    get_fp_apmode()
    {
        return _fp_apmode;
    }
    std::string
    get_ptr_size()
    {
        return _ptr_size;
    }
    std::string
    get_ptr_sign()
    {
        return _ptr_sign;
    }
    bool
    get_ptr_apmode()
    {
        return _ptr_apmode;
    }
    uint32_t
    get_limit()
    {
        return _limit;
    }
    std::string
    get_power_units()
    {
        return _power_units;
    }
    std::string
    get_energy_units()
    {
        return _energy_units;
    }
    std::string
    get_time_units()
    {
        return _time_units;
    }
    std::string
    get_area_units()
    {
        return _area_units;
    }
    uint32_t
    get_fu_latency()
    {
        return _fu_latency;
    }
    double
    get_internal_power()
    {
        return _internal_power;
    }
    double
    get_switch_power()
    {
        return _switch_power;
    }
    double
    get_dynamic_power()
    {
        return _dynamic_power;
    }
    double
    get_dynamic_energy()
    {
        return _dynamic_energy;
    }
    double
    get_leakage_power()
    {
        return _leakage_power;
    }
    double
    get_area()
    {
        return _area;
    }
    double
    get_path_delay()
    {
        return _path_delay;
    }
    bool
    is_available()
    {
        return (_available == 0) || (_in_use < _available);
    }
    void
    use_functional_unit()
    {
        _in_use++;
        _accept_count++;
        _cycle_accepts++;
        if (_in_use > _peak_busy_slots) {
            _peak_busy_slots = _in_use;
        }
        if (_in_use > _cycle_peak_in_use) {
            _cycle_peak_in_use = _in_use;
        }
    }
    void
    clear_functional_unit()
    {
        if (_in_use > 0) {
            _in_use--;
        }
    }
    void
    set_functional_unit_limit(uint64_t available)
    {
        _limit = available;
        _available = available;
    }
    void
    inc_functional_unit_limit()
    {
        _limit++;
        _available++;
    }
    uint64_t
    get_functional_unit_limit()
    {
        return _available;
    }

    uint64_t
    get_in_use()
    {
        return _in_use;
    }
    void
    note_deny()
    {
        _deny_count++;
        _cycle_denies++;
    }
    void
    sample_cycle()
    {
        _busy_slot_sum += _in_use;
        if (_in_use > 0) {
            _active_cycles++;
        }
        if (_in_use > _peak_busy_slots) {
            _peak_busy_slots = _in_use;
        }
        _cycle_accepts = 0;
        _cycle_denies = 0;
        _cycle_peak_in_use = 0;
    }
    void
    reset_runtime_stats()
    {
        _accept_count = 0;
        _deny_count = 0;
        _busy_slot_sum = 0;
        _active_cycles = 0;
        _peak_busy_slots = 0;
        _cycle_accepts = 0;
        _cycle_denies = 0;
        _cycle_peak_in_use = 0;
    }
    uint64_t
    get_accept_count() const
    {
        return _accept_count;
    }
    uint64_t
    get_deny_count() const
    {
        return _deny_count;
    }
    uint64_t
    get_busy_slot_sum() const
    {
        return _busy_slot_sum;
    }
    uint64_t
    get_active_cycles() const
    {
        return _active_cycles;
    }
    uint64_t
    get_peak_busy_slots() const
    {
        return _peak_busy_slots;
    }
    uint64_t
    get_cycle_accepts() const
    {
        return _cycle_accepts;
    }
    uint64_t
    get_cycle_denies() const
    {
        return _cycle_denies;
    }
    uint64_t
    get_cycle_peak_in_use() const
    {
        return _cycle_peak_in_use;
    }
};
#endif // __HWMODEL_FUNCTIONAL_UNIT_BASE_HH__
