/*
 * Copyright (c) 2013 ARM Limited
 * Copyright (c) 2022-2023 The University of Edinburgh
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
 * Copyright (c) 2020 Inria
 * All rights reserved.
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

#ifndef __SIM_PROBE_PROBE_LISTENER_OBJECT_HH__
#define __SIM_PROBE_PROBE_LISTENER_OBJECT_HH__

#include <vector>

#include "sim/probe/probe.hh"
#include "sim/sim_object.hh"

namespace gem5
{

struct ProbeListenerObjectParams;

/**
 * This class is a minimal wrapper around SimObject. It is used to declare
 * a python derived object that can be added as a ProbeListener to any other
 * SimObject.
 *
 * It instantiates manager from a call to Parent.any.
 * The vector of listeners is used simply to hold onto listeners until the
 * ProbeListenerObject is destroyed.
 */
class ProbeListenerObject : public SimObject
{
  protected:
    ProbeManager *manager;
    std::vector<ProbeListenerPtr<>> listeners;

  public:
    ProbeListenerObject(const ProbeListenerObjectParams &params);
    virtual ~ProbeListenerObject();
    ProbeManager *getProbeManager() { return manager; }

    template <typename T, typename... Args>
    void connectListener(Args &&...args)
    {
        listeners.push_back(
            getProbeManager()->connect<T>(std::forward<Args>(args)...));
    }
};

} // namespace gem5

#endif//__SIM_PROBE_PROBE_LISTENER_OBJECT_HH__
