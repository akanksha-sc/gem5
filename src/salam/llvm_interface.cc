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

#include "salam/llvm_interface.hh"

#include <sstream>

#include "sim/core.hh"

LLVMInterface::LLVMInterface(const LLVMInterfaceParams &p)
    : AccComputeUnit(p),
      filename(p.in_file),
      topName(p.top_name),
      scheduling_threshold(p.sched_threshold),
      windowStatsEnable(p.window_stats_enable),
      windowSize(p.window_size),
      lockstep(p.lockstep_mode)
{
    dbg = comm->debug();
}

std::shared_ptr<SALAM::Value>
createClone(const std::shared_ptr<SALAM::Value> &b)
{
    std::shared_ptr<SALAM::Value> clone = b->clone();
    return clone;
}

void
LLVMInterface::ActiveFunction::scheduleBB(
    std::shared_ptr<SALAM::BasicBlock> bb)
{
    auto schedulingStart = std::chrono::high_resolution_clock::now();
    if (dbg) {
        DPRINTFS(Runtime, owner, "|---[Schedule BB - UID:%i ]\n",
                 bb->getUID());
    }
    bool needToScheduleBranch = false;
    std::shared_ptr<SALAM::BasicBlock> nextBB;
    auto instruction_list = *(bb->Instructions());
    for (auto inst : instruction_list) {
        std::shared_ptr<SALAM::Instruction> clone_inst = inst->clone();
        clone_inst->setHWInterface(owner->hw);
        if (dbg) {
            DPRINTFS(Runtime, owner, "\t\t Instruction Cloned [UID: %d] \n",
                     inst->getUID());
        }
        if (clone_inst->isBr()) {
            if (dbg) {
                DPRINTFS(Runtime, owner, "\t\t Branch Instruction Found\n");
            }
            auto branch = std::dynamic_pointer_cast<SALAM::Br>(clone_inst);
            if (branch && !(branch->isConditional())) {
                if (dbg) {
                    DPRINTFS(
                        Runtime, owner,
                        "\t\t Unconditional Branch, Scheduling Next BB\n");
                }
                nextBB = branch->getTarget();
                if (dbg) {
                    DPRINTFS(RuntimeCompute, owner,
                             "\t\t Branching to %s from %s\n",
                             nextBB->getIRStub(), bb->getIRStub());
                }
                needToScheduleBranch = true;
            } else {
                findDynamicDeps(clone_inst);
                reservation.push_back(clone_inst);
            }
        } else {
            if (clone_inst->isPhi()) {
                if (dbg) {
                    DPRINTFS(Runtime, owner, "\t\t Phi Instruction Found\n");
                }
                auto phi = std::dynamic_pointer_cast<SALAM::Phi>(clone_inst);
                if (phi) {
                    phi->setPrevBB(previousBB);
                }
            }
            findDynamicDeps(clone_inst);
            reservation.push_back(clone_inst);
        }
    }
    previousBB = bb;
    auto schedulingStop = std::chrono::high_resolution_clock::now();
    owner->addSchedulingTime(schedulingStop - schedulingStart);
    if (needToScheduleBranch) {
        scheduleBB(nextBB);
    }
}

int
LLVMInterface::totalReservationDepth() const
{
    int total = 0;
    for (const auto &af : activeFunctions) {
        total += af.reservationDepth();
    }
    return total;
}

int
LLVMInterface::totalReadDepth() const
{
    int total = 0;
    for (const auto &af : activeFunctions) {
        total += af.readDepth();
    }
    return total;
}

int
LLVMInterface::totalWriteDepth() const
{
    int total = 0;
    for (const auto &af : activeFunctions) {
        total += af.writeDepth();
    }
    return total;
}

int
LLVMInterface::totalComputeDepth() const
{
    int total = 0;
    for (const auto &af : activeFunctions) {
        total += af.computeDepth();
    }
    return total;
}

void
LLVMInterface::snapshotQueueDepthStart()
{
    const int r = totalReservationDepth();
    const int rd = totalReadDepth();
    const int wr = totalWriteDepth();
    const int cmp = totalComputeDepth();

    tick_hw_cycle_stats.reservationDepth = r;
    tick_hw_cycle_stats.readQueueDepth = rd;
    tick_hw_cycle_stats.writeQueueDepth = wr;
    tick_hw_cycle_stats.computeQueueDepth = cmp;

    tick_hw_cycle_stats.reservationDepthStart = r;
    tick_hw_cycle_stats.readQueueDepthStart = rd;
    tick_hw_cycle_stats.writeQueueDepthStart = wr;
    tick_hw_cycle_stats.computeQueueDepthStart = cmp;

    tick_hw_cycle_stats.reservationDepthPeak = r;
    tick_hw_cycle_stats.readQueueDepthPeak = rd;
    tick_hw_cycle_stats.writeQueueDepthPeak = wr;
    tick_hw_cycle_stats.computeQueueDepthPeak = cmp;
}

void
LLVMInterface::sampleQueueDepthPeaks()
{
    tick_hw_cycle_stats.reservationDepthPeak = std::max(
        tick_hw_cycle_stats.reservationDepthPeak, totalReservationDepth());
    tick_hw_cycle_stats.readQueueDepthPeak =
        std::max(tick_hw_cycle_stats.readQueueDepthPeak, totalReadDepth());
    tick_hw_cycle_stats.writeQueueDepthPeak =
        std::max(tick_hw_cycle_stats.writeQueueDepthPeak, totalWriteDepth());
    tick_hw_cycle_stats.computeQueueDepthPeak = std::max(
        tick_hw_cycle_stats.computeQueueDepthPeak, totalComputeDepth());
}

void
LLVMInterface::snapshotQueueDepthEnd()
{
    tick_hw_cycle_stats.reservationDepthEnd = totalReservationDepth();
    tick_hw_cycle_stats.readQueueDepthEnd = totalReadDepth();
    tick_hw_cycle_stats.writeQueueDepthEnd = totalWriteDepth();
    tick_hw_cycle_stats.computeQueueDepthEnd = totalComputeDepth();
}

void
LLVMInterface::ActiveFunction::processQueues()
{
    auto queueStart = std::chrono::high_resolution_clock::now();

    owner->cycleSignals.anyReservationWork |= !reservation.empty();
    owner->cycleSignals.reservationNonEmpty |= !reservation.empty();
    owner->cycleSignals.readQueueNonEmpty |= !readQueue.empty();
    owner->cycleSignals.writeQueueNonEmpty |= !writeQueue.empty();
    owner->cycleSignals.computeQueueNonEmpty |= !computeQueue.empty();

    if (dbg) {
        DPRINTFS(Runtime, owner, "\t\t  |-[Process Queues]--------\n");
        DPRINTFS(RuntimeQueues, owner,
                 "\t\t[Runtime Queue] Reservation:%d, Compute:%d, Read:%d, "
                 "Write:%d\n",
                 reservation.size(), computeQueue.size(), readQueue.size(),
                 writeQueue.size());
    }
    // First pass, computeQueue is empty
    for (auto queue_iter = computeQueue.begin();
         queue_iter != computeQueue.end();) {
        if (dbg) {
            DPRINTFS(Runtime, owner, "\n\t\t %s \n\t\t %s%s%s%d%s \n",
                     " |-[Compute Queue]--------------", " | Instruction: ",
                     llvm::Instruction::getOpcodeName(
                         (queue_iter->second)->getOpode()),
                     " | UID[", (queue_iter->first), "]");
        }
        if ((queue_iter->second)->commit()) {
            (queue_iter->second)->reset();
            queue_iter = computeQueue.erase(queue_iter);
            owner->tick_hw_cycle_stats.computeCommitted++;
            owner->dynComputeCommitted++;
            owner->dynInstsCommitted++;
        } else {
            ++queue_iter;
        }
    }

    owner->cycleSignals.anyOutstandingCompute |= !computeQueue.empty();
    owner->cycleSignals.computeQueueNonEmpty |= !computeQueue.empty();

    if (canReturn()) {
        // Handle function return
        if (dbg) {
            DPRINTFS(Runtime, owner, "[[Function Return]]\n\n");
        }
        if (caller != nullptr) {
            // Signal the calling instruction
            if (caller->getSize() > 0) {
                auto retInst = reservation.front();
                auto retOperand = retInst->getOperands()->front();
                caller->setRegisterValue(retOperand.getOpRegister());
            }
            func->removeInstance();
            caller->commit();
            owner->dynCallsCommitted++;
            owner->dynInstsCommitted++;
            owner->tick_hw_cycle_stats.callsCommitted++;
        }
        owner->cycleSignals.anyOutstandingMemory |=
            (!readQueue.empty() || !writeQueue.empty());
        owner->cycleSignals.readQueueNonEmpty |= !readQueue.empty();
        owner->cycleSignals.writeQueueNonEmpty |= !writeQueue.empty();
        owner->cycleSignals.computeQueueNonEmpty |= !computeQueue.empty();
        owner->cycleSignals.anyOutstandingCompute |= !computeQueue.empty();
        owner->cycleSignals.unissuedMemoryReq |= hasUnissuedMemRequests();

        returned = true;
        return;

    } else if (lockstepReady()) {
        // TODO: Look into for_each here
        for (auto queue_iter = reservation.begin();
             queue_iter != reservation.end();) {
            if (owner->debug()) {
                if (dbg) {
                    DPRINTFS(Runtime, owner, "Debug Breakpoint");
                }
            }
            auto inst = *queue_iter;
            if (dbg) {
                DPRINTFS(Runtime, owner, "\n\t\t %s \n\t\t %s%s%s%d%s \n",
                         " |-[Reserve Queue]--------------",
                         " | Instruction: ",
                         llvm::Instruction::getOpcodeName((inst)->getOpode()),
                         " | UID[", (inst)->getUID(), "]");
            }
            if (!(inst)->isReturn()) {
                if ((inst)->isTerminator() &&
                    reservation.size() >= scheduling_threshold) {
                    owner->cycleSignals.thresholdBlocked = true;
                    ++queue_iter;
                } else if (((inst)->ready()) && !uidActive((inst)->getUID())) {
                    if ((inst)->isLoad()) {
                        // RAW protection to ensure a writeback finishes
                        // before reading that location
                        owner->cycleSignals.anyReadyMemory = true;
                        if (inst->isLoadingInternal()) {
                            launchRead(inst);
                            if (dbg) {
                                DPRINTFS(
                                    Runtime, owner,
                                    "\t\t  |-Erase From Queue: %s - UID[%i]\n",
                                    llvm::Instruction::getOpcodeName(
                                        (*queue_iter)->getOpode()),
                                    (*queue_iter)->getUID());
                            }
                            queue_iter = reservation.erase(queue_iter);
                        } else if (!writeActive(inst->getPtrOperandValue(0))) {
                            launchRead(inst);
                            if (dbg) {
                                DPRINTFS(
                                    Runtime, owner,
                                    "\t\t  |-Erase From Queue: %s - UID[%i]\n",
                                    llvm::Instruction::getOpcodeName(
                                        (*queue_iter)->getOpode()),
                                    (*queue_iter)->getUID());
                            }
                            queue_iter = reservation.erase(queue_iter);
                        } else {
                            auto activeWrite =
                                getActiveWrite(inst->getPtrOperandValue(0));
                            inst->addRuntimeDependency(activeWrite);
                            activeWrite->addRuntimeUser(inst);
                            ++queue_iter;
                            owner->cycleSignals.loadRawHazard = true;
                            owner->cycleSignals.dependencyBlocked = true;
                        }
                    } else if ((inst)->isStore()) {
                        // WAR Protection to insure reading
                        // finishes before a write
                        launchWrite(inst);
                        if (dbg) {
                            DPRINTFS(
                                Runtime, owner,
                                "\t\t  |-Erase From Queue: %s - UID[%i]\n",
                                llvm::Instruction::getOpcodeName(
                                    (*queue_iter)->getOpode()),
                                (*queue_iter)->getUID());
                        }
                        queue_iter = reservation.erase(queue_iter);
                    } else if ((inst)->isLatchingBrExiting() &&
                               ((reservation.size() > 1) || !queuesClear())) {
                        ++queue_iter;
                    } else if ((inst)->isTerminator()) {
                        owner->dynInstsIssued++;
                        (inst)->launch();
                        auto nextBB = inst->getTarget();
                        if (dbg) {
                            DPRINTFS(RuntimeCompute, owner,
                                     "\t\t Branching to %s from %s\n",
                                     nextBB->getIRStub(),
                                     previousBB->getIRStub());
                        }
                        scheduleBB(nextBB);
                        if (dbg) {
                            DPRINTFS(
                                Runtime, owner,
                                "\t\t  | Branch Scheduled: %s - UID[%i]\n",
                                llvm::Instruction::getOpcodeName(
                                    (inst)->getOpode()),
                                (inst)->getUID());
                        }
                        (inst)->commit();
                        owner->dynInstsCommitted++;
                        if (dbg) {
                            DPRINTFS(
                                Runtime, owner,
                                "\t\t  |-Erase From Queue: %s - UID[%i]\n",
                                llvm::Instruction::getOpcodeName(
                                    (*queue_iter)->getOpode()),
                                (*queue_iter)->getUID());
                        }
                        queue_iter = reservation.erase(queue_iter);
                    } else if ((*queue_iter)->isCall()) {
                        auto callInst =
                            std::dynamic_pointer_cast<SALAM::Call>(inst);
                        assert(callInst);
                        auto calleeValue = callInst->getCalleeValue();
                        auto callee =
                            std::dynamic_pointer_cast<SALAM::Function>(
                                calleeValue);
                        assert(callee);
                        if (callee->canLaunch()) {
                            owner->dynInstsIssued++;
                            owner->dynCallsIssued++;
                            owner->tick_hw_cycle_stats.callsIssued++;

                            owner->launchFunction(callee, callInst);
                            computeQueue.insert({(inst)->getUID(), inst});

                            if (dbg) {
                                DPRINTFS(
                                    Runtime, owner,
                                    "\t\t  |-Erase From Queue: %s - UID[%i]\n",
                                    llvm::Instruction::getOpcodeName(
                                        (*queue_iter)->getOpode()),
                                    (*queue_iter)->getUID());
                            }
                            queue_iter = reservation.erase(queue_iter);
                        } else {
                            owner->cycleSignals.callBlocked = true;
                            ++queue_iter;
                        }
                    } else {
                        auto computeStart =
                            std::chrono::high_resolution_clock::now();

                        owner->cycleSignals.anyReadyCompute = true;

                        owner->dynComputeLaunchAttempts++;
                        owner->tick_hw_cycle_stats.computeLaunchAttempts++;

                        auto launch_status = (inst)->launch();

                        switch (launch_status) {
                            case SALAM::Instruction::LaunchStatus::DeniedNoFU:
                                owner->cycleSignals.fuDenied = true;
                                if (dbg) {
                                    DPRINTFS(
                                        Runtime, owner,
                                        "\t\t  | Launch denied (FU busy): "
                                        "%s - UID[%i]\n",
                                        llvm::Instruction::getOpcodeName(
                                            (inst)->getOpode()),
                                        (inst)->getUID());
                                }
                                ++queue_iter;
                                break;

                            case SALAM::Instruction::LaunchStatus::
                                LaunchedInFlight:
                                owner->dynInstsIssued++;
                                owner->dynComputeLaunched++;
                                if (dbg) {
                                    DPRINTFS(
                                        Runtime, owner,
                                        "\t\t  | Added to Comp Queue: %s - "
                                        "UID[%i]\n",
                                        llvm::Instruction::getOpcodeName(
                                            (inst)->getOpode()),
                                        (inst)->getUID());
                                }
                                computeQueue.insert({(inst)->getUID(), inst});
                                owner->tick_hw_cycle_stats
                                    .computeLaunchesAccepted++;

                                if (dbg) {
                                    DPRINTFS(Runtime, owner,
                                             "\t\t  | Erase From Queue: %s - "
                                             "UID[%i]\n",
                                             llvm::Instruction::getOpcodeName(
                                                 (*queue_iter)->getOpode()),
                                             (*queue_iter)->getUID());
                                }
                                queue_iter = reservation.erase(queue_iter);
                                break;

                            case SALAM::Instruction::LaunchStatus::
                                LaunchedAndCommitted:
                                owner->dynInstsIssued++;
                                owner->dynComputeLaunched++;
                                owner->dynComputeCommitted++;
                                owner->dynInstsCommitted++;
                                owner->tick_hw_cycle_stats
                                    .computeLaunchesAccepted++;
                                owner->tick_hw_cycle_stats.computeCommitted++;
                                if (dbg) {
                                    DPRINTFS(Runtime, owner,
                                             "\t\t  | Immediate compute "
                                             "commit: %s - "
                                             "UID[%i]\n",
                                             llvm::Instruction::getOpcodeName(
                                                 (*queue_iter)->getOpode()),
                                             (*queue_iter)->getUID());
                                    DPRINTFS(Runtime, owner,
                                             "\t\t  | Erase From Queue: %s - "
                                             "UID[%i]\n",
                                             llvm::Instruction::getOpcodeName(
                                                 (*queue_iter)->getOpode()),
                                             (*queue_iter)->getUID());
                                }
                                queue_iter = reservation.erase(queue_iter);
                                break;
                        }

                        auto computeStop =
                            std::chrono::high_resolution_clock::now();
                        owner->addComputeTime(computeStop - computeStart);
                    }
                } else {
                    owner->cycleSignals.dependencyBlocked = true;
                    ++queue_iter;
                }
            } else {
                ++queue_iter;
            }
        }
    } else {
        owner->cycleSignals.lockstepBlocked = true;
    }

    owner->cycleSignals.anyOutstandingMemory |=
        (!readQueue.empty() || !writeQueue.empty());
    owner->cycleSignals.readQueueNonEmpty |= !readQueue.empty();
    owner->cycleSignals.writeQueueNonEmpty |= !writeQueue.empty();
    owner->cycleSignals.computeQueueNonEmpty |= !computeQueue.empty();
    owner->cycleSignals.anyOutstandingCompute |= !computeQueue.empty();
    owner->cycleSignals.unissuedMemoryReq |= hasUnissuedMemRequests();

    auto queueStop = std::chrono::high_resolution_clock::now();
    owner->addQueueTime(queueStop - queueStart);
}

/*****************************************************************************
 CN Scheduling

 As CNs are scheduled they are added to an in-flight queue depending on
 operation type.
 Loads and Stores are maintained in separate queues, and are committed
 by the comm_interface.
 Branch and phi instructions evaluate and commit immediately.
 All other CN types are added to an in-flight compute queue.

 Each tick we must first check our in-flight compute queue.
 Each node should have its cycle count incremented,
 and should commit if max cycle is reached.

 New CNs are added to the reservation table whenever a new BB is encountered.
 This may occur during device init, or when a br op commits.
 For each CN in a BB we reset the CN, evaluate if it is a phi or uncond br,
 and add it to our reservation table otherwise.
*****************************************************************************/

void
LLVMInterface::captureCommInterfaceCycleStats()
{
    const auto &cs = comm->getCycleIfaceStats();
    tick_hw_cycle_stats.hadReadIssueBackpressure = cs.hadReadBackpressure;
    tick_hw_cycle_stats.hadWriteIssueBackpressure = cs.hadWriteBackpressure;
    tick_hw_cycle_stats.hadReadRetry = cs.sawReadRetry;
    tick_hw_cycle_stats.hadWriteRetry = cs.sawWriteRetry;

    for (size_t t = 0; t < kNumTargetClasses; ++t) {
        for (size_t a = 0; a < kNumAccessKinds; ++a) {
            tick_hw_cycle_stats.memOps[t][a] += cs.issuedMemOps[t][a];
            tick_hw_cycle_stats.memBytes[t][a] += cs.issuedMemBytes[t][a];
            tick_hw_cycle_stats.memAcceptedOps[t][a] +=
                cs.acceptedMemOps[t][a];
            tick_hw_cycle_stats.memAcceptedBytes[t][a] +=
                cs.acceptedMemBytes[t][a];
        }
    }
}

void
LLVMInterface::captureFuCycleStats()
{
    hw->copyFuCycleStats(tick_hw_cycle_stats);
    hw->sampleFuCycle();
}

void
LLVMInterface::captureComputeMemoryOverlapStats()
{
    tick_hw_cycle_stats.hadComputeAndMemoryOutstanding =
        cycleSignals.anyOutstandingCompute &&
        cycleSignals.anyOutstandingMemory;
    tick_hw_cycle_stats.hadComputeAndMemoryReady =
        cycleSignals.anyReadyCompute && cycleSignals.anyReadyMemory;
}

void
LLVMInterface::updateWindowStats(CycleCause cause)
{
    curWindow.cycles++;
    switch (cause) {
        case CycleCause::UsefulCompute:
            curWindow.usefulCompute++;
            break;
        case CycleCause::UsefulMemory:
            curWindow.usefulMemory++;
            break;
        case CycleCause::UsefulControl:
            curWindow.usefulControl++;
            break;
        case CycleCause::DependencyStall:
            curWindow.depStall++;
            break;
        case CycleCause::FuCapacityStall:
            curWindow.fuStall++;
            break;
        case CycleCause::ComputeLatencyWait:
            curWindow.cmpWait++;
            break;
        case CycleCause::MemoryServiceWait:
            curWindow.memWait++;
            break;
        case CycleCause::MemoryIssueBackpressure:
            curWindow.memBpWait++;
            break;
        case CycleCause::ComputeAndMemoryOutstandingWait:
            curWindow.bothWait++;
            break;
        case CycleCause::SchedulingBlocked:
            curWindow.schedBlocked++;
            break;
        case CycleCause::Idle:
            curWindow.idle++;
            break;
    }
}

void
LLVMInterface::emitWindowSummary() const
{
    std::cout << "SALAM_WINDOW"
              << " name=" << name() << " window=" << windowIndex
              << " cycles=" << curWindow.cycles
              << " useful_compute=" << curWindow.usefulCompute
              << " useful_memory=" << curWindow.usefulMemory
              << " useful_control=" << curWindow.usefulControl
              << " dep_stall=" << curWindow.depStall
              << " fu_stall=" << curWindow.fuStall
              << " cmp_wait=" << curWindow.cmpWait
              << " mem_wait=" << curWindow.memWait
              << " mem_bp_wait=" << curWindow.memBpWait
              << " both_wait=" << curWindow.bothWait
              << " sched_blocked=" << curWindow.schedBlocked
              << " idle=" << curWindow.idle << std::endl;
}

void
LLVMInterface::resetWindowStats()
{
    curWindow = WindowStats{};
    windowIndex++;
}

void
LLVMInterface::tick()
{
    auto tickStart = std::chrono::high_resolution_clock::now();

    const bool hadActiveWork = !activeFunctions.empty();
    const uint64_t instsIssuedStart = dynInstsIssued;
    const uint64_t instsCommittedStart = dynInstsCommitted;
    const uint64_t loadsIssuedStart = dynLoadsIssued;
    const uint64_t loadsCommittedStart = dynLoadsCommitted;
    const uint64_t storesIssuedStart = dynStoresIssued;
    const uint64_t storesCommittedStart = dynStoresCommitted;
    const uint64_t computeLaunchedStart = dynComputeLaunched;
    const uint64_t computeCommittedStart = dynComputeCommitted;
    cycleSignals.reset();

    if (dbg) {
        DPRINTF(LLVMInterface, "\n%s\n%s %d\n%s\n",
                "*************************************************************"
                "*****",
                "   Cycle", cycle,
                "*************************************************************"
                "*****");
    }
    cycle++;

    if (hw->hw_statistics->use_cycle_tracking()) {
        tick_hw_cycle_stats.reset();
        tick_hw_cycle_stats.cycle = cycle;
        snapshotQueueDepthStart();
    }

    // Process Queues in Active Functions
    for (auto func_iter = activeFunctions.begin();
         func_iter != activeFunctions.end();) {
        func_iter->processQueues();
        if (hw->hw_statistics->use_cycle_tracking()) {
            sampleQueueDepthPeaks();
        }
        if (!(func_iter->hasReturned())) {
            func_iter++;
        } else {
            func_iter = activeFunctions.erase(func_iter);
            if (hw->hw_statistics->use_cycle_tracking()) {
                sampleQueueDepthPeaks();
            }
        }
    }

    const bool computeProgress =
        (dynComputeLaunched != computeLaunchedStart) ||
        (dynComputeCommitted != computeCommittedStart);

    const bool memoryProgress = (dynLoadsIssued != loadsIssuedStart) ||
                                (dynLoadsCommitted != loadsCommittedStart) ||
                                (dynStoresIssued != storesIssuedStart) ||
                                (dynStoresCommitted != storesCommittedStart);

    const bool anyInstProgress = (dynInstsIssued != instsIssuedStart) ||
                                 (dynInstsCommitted != instsCommittedStart);

    const bool controlProgress =
        anyInstProgress && !computeProgress && !memoryProgress;

    const bool memoryBackpressure = cycleSignals.unissuedMemoryReq ||
                                    comm->hadIssueBackpressureThisCycle();

    // Overlapping subreasons / occupancy-like cycle counters.
    if (cycleSignals.reservationNonEmpty) {
        reservationNonEmptyCycles++;
    }
    if (cycleSignals.readQueueNonEmpty) {
        readQueueNonEmptyCycles++;
    }
    if (cycleSignals.writeQueueNonEmpty) {
        writeQueueNonEmptyCycles++;
    }
    if (cycleSignals.computeQueueNonEmpty) {
        computeQueueNonEmptyCycles++;
    }
    if (cycleSignals.loadRawHazard) {
        loadRawHazardCycles++;
    }
    if (cycleSignals.callBlocked) {
        callWaitCycles++;
    }
    if (cycleSignals.thresholdBlocked) {
        thresholdBlockedCycles++;
    }
    if (cycleSignals.lockstepBlocked) {
        lockstepBlockedCycles++;
    }
    if (memoryBackpressure) {
        memoryBackpressureCycles++;
    }
    if (cycleSignals.unissuedMemoryReq) {
        unissuedMemoryReqCycles++;
    }
    if (comm->allPortsStalledThisCycle()) {
        allPortsStalledCycles++;
    }
    if (comm->sawRetryThisCycle()) {
        portRetryCycles++;
    }

    CycleCause cause = CycleCause::Idle;

    if (!hadActiveWork && !cycleSignals.anyReservationWork &&
        !cycleSignals.anyOutstandingMemory &&
        !cycleSignals.anyOutstandingCompute && !anyInstProgress) {
        cause = CycleCause::Idle;
    } else if (computeProgress) {
        // Compute takes precedence over memory if both progressed this cycle.
        cause = CycleCause::UsefulCompute;
    } else if (memoryProgress) {
        cause = CycleCause::UsefulMemory;
    } else if (controlProgress) {
        cause = CycleCause::UsefulControl;
    } else if (cycleSignals.lockstepBlocked || cycleSignals.thresholdBlocked ||
               cycleSignals.callBlocked) {
        cause = CycleCause::SchedulingBlocked;
    } else if (cycleSignals.fuDenied) {
        cause = CycleCause::FuCapacityStall;
    } else if (cycleSignals.anyOutstandingCompute &&
               cycleSignals.anyOutstandingMemory) {
        cause = CycleCause::ComputeAndMemoryOutstandingWait;
    } else if (cycleSignals.anyOutstandingCompute) {
        cause = CycleCause::ComputeLatencyWait;
    } else if (memoryBackpressure) {
        cause = CycleCause::MemoryIssueBackpressure;
    } else if (cycleSignals.anyOutstandingMemory) {
        cause = CycleCause::MemoryServiceWait;
    } else if (cycleSignals.dependencyBlocked || cycleSignals.loadRawHazard) {
        cause = CycleCause::DependencyStall;
    } else {
        cause = CycleCause::Idle;
    }

    switch (cause) {
        case CycleCause::UsefulCompute:
            usefulComputeCycles++;
            break;
        case CycleCause::UsefulMemory:
            usefulMemoryCycles++;
            break;
        case CycleCause::UsefulControl:
            usefulControlCycles++;
            break;
        case CycleCause::DependencyStall:
            dependencyStallCycles++;
            break;
        case CycleCause::FuCapacityStall:
            fuCapacityStallCycles++;
            break;
        case CycleCause::ComputeLatencyWait:
            computeLatencyWaitCycles++;
            break;
        case CycleCause::MemoryServiceWait:
            memoryServiceWaitCycles++;
            break;
        case CycleCause::MemoryIssueBackpressure:
            memoryIssueBackpressureCycles++;
            break;
        case CycleCause::ComputeAndMemoryOutstandingWait:
            computeAndMemoryOutstandingWaitCycles++;
            break;
        case CycleCause::SchedulingBlocked:
            schedulingBlockedCycles++;
            break;
        case CycleCause::Idle:
            idleCycles++;
            break;
    }

    if (windowStatsEnable) {
        updateWindowStats(cause);
        if (curWindow.cycles == windowSize) {
            emitWindowSummary();
            resetWindowStats();
        }
    }

    if (hadActiveWork) {
        const bool hadForwardProgress =
            (dynInstsIssued != instsIssuedStart) ||
            (dynInstsCommitted != instsCommittedStart);

        if (!hadForwardProgress) {
            stalls++;
        }
    }

    if (hw->hw_statistics->use_cycle_tracking()) {
        auto hwStart = std::chrono::high_resolution_clock::now();

        // Write exact owner-level cycle flags for this LLVMInterface tick.
        tick_hw_cycle_stats.hadLoadRawHazard = cycleSignals.loadRawHazard;
        tick_hw_cycle_stats.hadFuCapacityDeny = cycleSignals.fuDenied;
        tick_hw_cycle_stats.hadCallWait = cycleSignals.callBlocked;
        tick_hw_cycle_stats.hadThresholdBlock = cycleSignals.thresholdBlocked;
        tick_hw_cycle_stats.hadLockstepBlock = cycleSignals.lockstepBlocked;

        tick_hw_cycle_stats.hadIssueBackpressure = memoryBackpressure;
        tick_hw_cycle_stats.hadAllPortsStalled =
            comm->allPortsStalledThisCycle();
        tick_hw_cycle_stats.hadPortRetry = comm->sawRetryThisCycle();
        tick_hw_cycle_stats.hadUnissuedMemoryReq =
            cycleSignals.unissuedMemoryReq;

        tick_hw_cycle_stats.hadOutstandingMemory =
            cycleSignals.anyOutstandingMemory;
        tick_hw_cycle_stats.hadOutstandingCompute =
            cycleSignals.anyOutstandingCompute;
        tick_hw_cycle_stats.hadReadyMemory = cycleSignals.anyReadyMemory;
        tick_hw_cycle_stats.hadReadyCompute = cycleSignals.anyReadyCompute;

        tick_hw_cycle_stats.reservationNonEmpty =
            cycleSignals.reservationNonEmpty;
        tick_hw_cycle_stats.readQueueNonEmpty = cycleSignals.readQueueNonEmpty;
        tick_hw_cycle_stats.writeQueueNonEmpty =
            cycleSignals.writeQueueNonEmpty;
        tick_hw_cycle_stats.computeQueueNonEmpty =
            cycleSignals.computeQueueNonEmpty;

        snapshotQueueDepthEnd();
        captureCommInterfaceCycleStats();
        captureFuCycleStats();
        captureComputeMemoryOverlapStats();

        foldPendingMemStatsIntoTickCycle();
        hw->hw_statistics->recordCycle(tick_hw_cycle_stats);
        clearPendingMemStats();

        auto hwStop = std::chrono::high_resolution_clock::now();
        addHWTime(hwStop - hwStart);
    } else {
        clearPendingMemStats();
    }

    if (activeFunctions.empty()) {
        // We are finished executing all functions.
        // Signal completion to the CommInterface
        running = false;
        finalize();
        return;
    }
    //////////////// Schedule Next Cycle ////////////////////////
    if (running && !tickEvent.scheduled()) {
        schedule(tickEvent, nextCycle());
    }
    auto tickStop = std::chrono::high_resolution_clock::now();
    simTime = simTime + (tickStop - tickStart);
}

/*****************************************************************************
- findDynamicDeps(std::list<std::shared_ptr<SALAM::Instructions>,
  std::shared_ptr<SALAM::Instruction>)
- only parse queue once for each instruction until all dependencies are found
- include self in dependency list
- Register dynamicUser/dynamicDependencies
  std::deque<std::shared_ptr<SALAM::Instructon> >
*****************************************************************************/
void // Add third argument, previous BB
LLVMInterface::ActiveFunction::findDynamicDeps(
    std::shared_ptr<SALAM::Instruction> inst)
{
    if (dbg) {
        DPRINTFS(Runtime, owner, "Linking Dynamic Dependencies [%s]\n",
                 llvm::Instruction::getOpcodeName(inst->getOpode()));
    }
    // The list of UIDs for any dependencies we want to find
    std::vector<uint64_t> dep_uids = inst->runtimeInitialize();

    // Find dependencies currently in queues

    // Reverse search the reservation queue because we want to link
    // only the last instance of each dep
    auto queue_iter = reservation.rbegin();
    while ((queue_iter != reservation.rend()) && !dep_uids.empty()) {
        auto queued_inst = *queue_iter;
        // Look at each instruction in runtime queue once
        for (auto dep_it = dep_uids.begin(); dep_it != dep_uids.end();) {
            // Check if any of the instruction to be scheduled dependencies
            // match the current instruction from queue
            if (queued_inst->getUID() == *dep_it) {
                // If dependency found, create two way link
                inst->addRuntimeDependency(queued_inst);
                queued_inst->addRuntimeUser(inst);
                dep_it = dep_uids.erase(dep_it);
            } else {
                dep_it++;
            }
        }
        queue_iter++;
    }

    // The other queues do not need to be reverse-searched since only 1
    // instance of any instruction can exist in them
    // Check the compute queue
    for (auto dep_it = dep_uids.begin(); dep_it != dep_uids.end();) {
        auto queue_iter = computeQueue.find(*dep_it);
        if (queue_iter != computeQueue.end()) {
            auto queued_inst = queue_iter->second;
            inst->addRuntimeDependency(queued_inst);
            queued_inst->addRuntimeUser(inst);
            dep_it = dep_uids.erase(dep_it);
        } else {
            dep_it++;
        }
    }
    // Check the memory read queue
    for (auto dep_it = dep_uids.begin(); dep_it != dep_uids.end();) {
        auto queue_iter = readQueue.find(*dep_it);
        if (queue_iter != readQueue.end()) {
            auto queued_inst = queue_iter->second;
            inst->addRuntimeDependency(queued_inst);
            queued_inst->addRuntimeUser(inst);
            dep_it = dep_uids.erase(dep_it);
        } else {
            dep_it++;
        }
    }

    // Check the memory write queue
    for (auto dep_it = dep_uids.begin(); dep_it != dep_uids.end();) {
        auto queue_iter = writeQueue.find(*dep_it);
        if (queue_iter != writeQueue.end()) {
            auto queued_inst = queue_iter->second;
            inst->addRuntimeDependency(queued_inst);
            queued_inst->addRuntimeUser(inst);
            dep_it = dep_uids.erase(dep_it);
        } else {
            dep_it++;
        }
    }

    // Fetch values for resolved dependencies,
    // static elements, and immediate values
    if (!dep_uids.empty()) {
        for (auto resolved : dep_uids) {
            // If this dependency exists, then lock value into operand
            inst->setOperandValue(resolved);
        }
    }
}

void
LLVMInterface::dumpModule(llvm::Module *M)
{
    M->print(llvm::outs(), nullptr);
    for (const llvm::Function &F : *M) {
        for (const llvm::BasicBlock &BB : F) {
            for (const llvm::Instruction &I : BB) {
                I.print(llvm::outs());
            }
        }
    }
}

void
LLVMInterface::constructStaticGraph()
{
    /************************************************************************
     Constructing the Static CDFG

     Parses LLVM file and creates the CDFG passed to runtime simulation
     engine.
    ************************************************************************/
    auto parseStart = std::chrono::high_resolution_clock::now();

    if (dbg) {
        DPRINTF(LLVMInterface, "Constructing Static Dependency Graph\n");
    }
    llvm::StringRef file = filename;
    std::unique_ptr<llvm::LLVMContext> context(new llvm::LLVMContext());
    std::unique_ptr<llvm::SMDiagnostic> error(new llvm::SMDiagnostic());
    std::unique_ptr<llvm::Module> m;
    std::unique_ptr<llvm::DominatorTree> dt(new llvm::DominatorTree());
    std::unique_ptr<llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>> loopInfo(
        new llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>());

    m = llvm::parseIRFile(file, *error, *context);
    if (!m) {
        panic("Error reading Module");
    }

    // Construct the LLVM::Value to SALAM::Value map
    uint64_t valueID = 0;
    SALAM::irvmap vmap;
    // Generate SALAM::Values for llvm::GlobalVariables
    DPRINTF(LLVMParse, "Instantiate SALAM::GlobalConstants\n");
    for (auto glob_iter = m->global_begin(); glob_iter != m->global_end();
         glob_iter++) {
        llvm::GlobalVariable &glb = *glob_iter;
        std::shared_ptr<SALAM::GlobalConstant> sglb =
            std::make_shared<SALAM::GlobalConstant>(valueID, this, debug());
        values.push_back(sglb);
        vmap.insert(SALAM::irvmaptype(&glb, sglb));
        valueID++;
    }
    // Generate SALAM::Functions
    DPRINTF(LLVMParse, "Instantiate SALAM::Functions\n");
    for (auto func_iter = m->begin(); func_iter != m->end(); func_iter++) {
        llvm::Function &func = *func_iter;
        std::shared_ptr<SALAM::Function> sfunc =
            std::make_shared<SALAM::Function>(valueID, this, debug());
        values.push_back(sfunc);
        functions.push_back(sfunc);
        vmap.insert(SALAM::irvmaptype(&func, sfunc));
        valueID++;
        // Generate args for SALAM:Functions
        DPRINTF(LLVMParse, "Instantiate SALAM::Functions::Arguments\n");
        for (auto arg_iter = func.arg_begin(); arg_iter != func.arg_end();
             arg_iter++) {
            llvm::Argument &arg = *arg_iter;
            std::shared_ptr<SALAM::Argument> sarg =
                std::make_shared<SALAM::Argument>(valueID, this, debug());
            values.push_back(sarg);
            vmap.insert(SALAM::irvmaptype(&arg, sarg));
            valueID++;
        }
        // Generate SALAM::BasicBlocks
        DPRINTF(LLVMParse, "Instantiate SALAM::Functions::BasicBlocks\n");
        for (auto bb_iter = func.begin(); bb_iter != func.end(); bb_iter++) {
            llvm::BasicBlock &bb = *bb_iter;
            std::shared_ptr<SALAM::BasicBlock> sbb =
                std::make_shared<SALAM::BasicBlock>(valueID, this, debug());
            values.push_back(sbb);
            vmap.insert(SALAM::irvmaptype(&bb, sbb));
            valueID++;
            // Generate SALAM::Instructions
            DPRINTF(
                LLVMParse,
                "Instantiate SALAM::Functions::BasicBlocks::Instructions\n");
            for (auto inst_iter = bb.begin(); inst_iter != bb.end();
                 inst_iter++) {
                llvm::Instruction &inst = *inst_iter;
                std::shared_ptr<SALAM::Instruction> sinst =
                    createInstruction(&inst, valueID);
                values.push_back(sinst);
                vmap.insert(SALAM::irvmaptype(&inst, sinst));
                valueID++;
            }
        }
    }

    // Use value map to initialize SALAM::Values
    DPRINTF(LLVMParse, "Initialize SALAM::GlobalConstants\n");
    for (auto glob_iter = m->global_begin(); glob_iter != m->global_end();
         glob_iter++) {
        llvm::GlobalVariable &glb = *glob_iter;
        std::shared_ptr<SALAM::Value> glbval = vmap.find(&glb)->second;
        assert(glbval);
        std::shared_ptr<SALAM::GlobalConstant> sglb =
            std::dynamic_pointer_cast<SALAM::GlobalConstant>(glbval);
        assert(sglb);
        sglb->initialize(&glb, &vmap, &values);
    }
    // Functions initialize BasicBlocks, which will initialize Instructions
    DPRINTF(LLVMParse, "Initialize SALAM::Functions\n");
    for (auto func_iter = m->begin(); func_iter != m->end(); func_iter++) {
        llvm::Function &func = *func_iter;
        std::shared_ptr<SALAM::Value> funcval = vmap.find(&func)->second;
        assert(funcval);
        std::shared_ptr<SALAM::Function> sfunc =
            std::dynamic_pointer_cast<SALAM::Function>(funcval);
        assert(sfunc);
        sfunc->initialize(&func, &vmap, &values, topName);
    }
    if (functions.size() == 1) {
        functions.front()->setTop(true);
    }

    // Detect Loop Latches
    for (auto func_iter = m->begin(); func_iter != m->end(); func_iter++) {
        llvm::Function &func = *func_iter;
        dt->recalculate(func);
        loopInfo->releaseMemory();
        loopInfo->analyze(*dt);
        for (auto loop = loopInfo->begin(); loop != loopInfo->end(); ++loop) {
            if (llvm::BasicBlock *exBB = (*loop)->getExitingBlock()) {
                auto latchingBr = exBB->getTerminator();
                auto mapIt = vmap.find(latchingBr);
                if (mapIt != vmap.end()) {
                    auto salamValue = mapIt->second;
                    if (std::shared_ptr<SALAM::Br> sBr =
                            std::dynamic_pointer_cast<SALAM::Br>(salamValue)) {
                        sBr->setLatching(true);
                    }
                }
            }
        }
    }
    auto parseStop = std::chrono::high_resolution_clock::now();
    setupTime = parseStop - parseStart;
}

void
LLVMInterface::launchRead(MemoryRequest *memReq, ActiveFunction *func)
{
    globalReadQueue.insert({memReq, func});
    comm->enqueueRead(memReq);
}

void
LLVMInterface::ActiveFunction::launchRead(
    std::shared_ptr<SALAM::Instruction> readInst)
{
    auto rdInst = std::dynamic_pointer_cast<SALAM::Load>(readInst);

    owner->dynInstsIssued++;
    owner->dynLoadsIssued++;
    owner->tick_hw_cycle_stats.loadsIssued++;

    if (rdInst->isLoadingInternal()) {
        rdInst->loadInternal();

        owner->dynInstsCommitted++;
        owner->dynLoadsCommitted++;
        owner->dynInternalLoadCompletions++;
        owner->tick_hw_cycle_stats.internalLoadCompletions++;
        owner->tick_hw_cycle_stats.loadsCommitted++;
    } else {
        auto memReq = (readInst)->createMemoryRequest();
        auto rd_uid = readInst->getUID();
        readQueue.insert({rd_uid, (readInst)});
        readQueueMap.insert({memReq, rd_uid});
        owner->launchRead(memReq, this);
    }
}

void
LLVMInterface::launchWrite(MemoryRequest *memReq, ActiveFunction *func)
{
    globalWriteQueue.insert({memReq, func});
    comm->enqueueWrite(memReq);
}

void
LLVMInterface::ActiveFunction::launchWrite(
    std::shared_ptr<SALAM::Instruction> writeInst)
{
    owner->dynInstsIssued++;
    owner->dynStoresIssued++;
    owner->tick_hw_cycle_stats.storesIssued++;

    auto memReq = (writeInst)->createMemoryRequest();
    trackWrite(memReq->getAddress(), writeInst);
    auto wr_uid = writeInst->getUID();
    writeQueue.insert({wr_uid, (writeInst)});
    writeQueueMap.insert({memReq, wr_uid});
    owner->launchWrite(memReq, this);
}

void
LLVMInterface::readCommit(MemoryRequest *req)
{
    /************************************************************************
     Commit Memory Read Request
    ************************************************************************/
    auto queue_iter = globalReadQueue.find(req);
    if (queue_iter != globalReadQueue.end()) {
        queue_iter->second->readCommit(req);
        DPRINTF(Runtime, "Global Read Commit\n");
        globalReadQueue.erase(queue_iter);
    } else {
        panic("No memory request in global read queue!");
    }
}

void
LLVMInterface::ActiveFunction::readCommit(MemoryRequest *req)
{
    /************************************************************************
     Commit Memory Read Request
    ************************************************************************/
    auto map_iter = readQueueMap.find(req);
    if (map_iter != readQueueMap.end()) {
        auto queue_iter = readQueue.find(map_iter->second);
        if (queue_iter != readQueue.end()) {
            auto load_inst = queue_iter->second;
            uint8_t *readBuff = req->getBuffer();
            load_inst->setRegisterValue(readBuff);
            load_inst->compute();
            if (dbg) {
                DPRINTFS(Runtime, owner, "Local Read Commit\n");
            }
            load_inst->commit();
            owner->dynLoadsCommitted++;
            owner->dynInstsCommitted++;
            // Async memory callback: defer exact commit-event accounting
            // until the current cycle record is finalized in
            // LLVMInterface::tick().
            owner->pendingLoadsCommitted++;
            readQueue.erase(queue_iter);
            readQueueMap.erase(map_iter);
        } else {
            panic("No memory request in read queue for function %u!",
                  func->getUID());
        }
    } else {
        panic("No memory request in read queue for function %u!",
              func->getUID());
    }
}

void
LLVMInterface::writeCommit(MemoryRequest *req)
{
    /*************************************************************************
     Commit Memory Write Request
    *************************************************************************/
    auto queue_iter = globalWriteQueue.find(req);
    if (queue_iter != globalWriteQueue.end()) {
        queue_iter->second->writeCommit(req);
        globalWriteQueue.erase(queue_iter);
    } else {
        panic("No memory request in global write queue!");
    }
}

void
LLVMInterface::ActiveFunction::writeCommit(MemoryRequest *req)
{
    /************************************************************************
     Commit Memory Write Request
    ************************************************************************/
    auto map_iter = writeQueueMap.find(req);
    if (map_iter != writeQueueMap.end()) {
        auto queue_iter = writeQueue.find(map_iter->second);
        if (queue_iter != writeQueue.end()) {
            queue_iter->second->commit();
            owner->dynStoresCommitted++;
            owner->dynInstsCommitted++;
            // Async memory callback: defer cycle accounting until the
            // current owner tick is finalized in LLVMInterface::tick().
            owner->pendingStoresCommitted++;
            Addr addressWritten = map_iter->first->getAddress();
            untrackWrite(addressWritten);
            writeQueue.erase(queue_iter);
            writeQueueMap.erase(map_iter);
        } else {
            panic("No memory request in write queue for function %u!",
                  func->getUID());
        }
    } else {
        panic("No memory request in write queue for function %u!",
              func->getUID());
    }
}

bool
LLVMInterface::hasCurrentInvocationData() const
{
    return cycle > 0 || dynInstsCommitted > 0 || dynLoadsCommitted > 0 ||
           dynStoresCommitted > 0 || dynComputeCommitted > 0 ||
           dynCallsCommitted > 0;
}

void
LLVMInterface::rollUpCurrentInvocationIntoAggregate()
{
    if (!hasCurrentInvocationData()) {
        return;
    }

    invocationCount++;

    aggCycles += cycle;
    aggDynInstsIssued += dynInstsIssued;
    aggDynInstsCommitted += dynInstsCommitted;
    aggDynLoadsIssued += dynLoadsIssued;
    aggDynLoadsCommitted += dynLoadsCommitted;
    aggDynStoresIssued += dynStoresIssued;
    aggDynStoresCommitted += dynStoresCommitted;
    aggDynComputeLaunchAttempts += dynComputeLaunchAttempts;
    aggDynComputeLaunched += dynComputeLaunched;
    aggDynComputeCommitted += dynComputeCommitted;
    aggDynCallsIssued += dynCallsIssued;
    aggDynCallsCommitted += dynCallsCommitted;

    aggUsefulComputeCycles += usefulComputeCycles;
    aggUsefulMemoryCycles += usefulMemoryCycles;
    aggUsefulControlCycles += usefulControlCycles;
    aggDependencyStallCycles += dependencyStallCycles;
    aggFuCapacityStallCycles += fuCapacityStallCycles;
    aggComputeLatencyWaitCycles += computeLatencyWaitCycles;
    aggMemoryServiceWaitCycles += memoryServiceWaitCycles;
    aggMemoryIssueBackpressureCycles += memoryIssueBackpressureCycles;
    aggComputeAndMemoryOutstandingWaitCycles +=
        computeAndMemoryOutstandingWaitCycles;
    aggSchedulingBlockedCycles += schedulingBlockedCycles;
    aggIdleCycles += idleCycles;

    aggUnissuedMemoryReqCycles += unissuedMemoryReqCycles;
    aggInternalLoadCompletions += dynInternalLoadCompletions;
    aggExternalLoadCompletions +=
        (dynLoadsCommitted - dynInternalLoadCompletions);
}

void
LLVMInterface::initialize()
{
    rollUpCurrentInvocationIntoAggregate();
    /************************************************************************
     Initialize the Runtime Engine

     Calls function that constructs the basic block list, initializes the
     reservation table and read, write, and compute queues.
     Set all data collection variables to zero.
    ************************************************************************/
    if (dbg) {
        DPRINTF(LLVMInterface, "Initializing LLVM Runtime Engine!\n");
    }
    setupTime = std::chrono::seconds(0);
    simTime = std::chrono::seconds(0);
    schedulingTime = std::chrono::seconds(0);
    queueProcessTime = std::chrono::seconds(0);
    computeTime = std::chrono::seconds(0);
    hwTime = std::chrono::seconds(0);
    constructStaticGraph();
    timeStart = std::chrono::high_resolution_clock::now();
    if (dbg) {
        DPRINTF(LLVMInterface, "=============================================="
                               "==================\n");
    }
    launchTopFunction();

    if (dbg) {
        DPRINTF(
            LLVMInterface, "\n%s\n%s\n%s\n",
            "****************************************************************",
            "*          Begin Runtime Simulation Computation Engine         *",
            "***************************************************************"
            "*");
    }
    running = true;
    cycle = 0;
    // Debug / sanity metric only
    // Counts compute-domain runtime cycles where there was active work
    // in-flight, but no forward progress this cycle
    stalls = 0;

    dynInstsIssued = 0;
    dynInstsCommitted = 0;
    dynLoadsIssued = 0;
    dynLoadsCommitted = 0;
    dynStoresIssued = 0;
    dynStoresCommitted = 0;
    dynComputeLaunchAttempts = 0;
    dynComputeLaunched = 0;
    dynComputeCommitted = 0;
    dynCallsIssued = 0;
    dynCallsCommitted = 0;
    dynInternalLoadCompletions = 0;

    cycleSignals.reset();

    usefulComputeCycles = 0;
    usefulMemoryCycles = 0;
    usefulControlCycles = 0;
    dependencyStallCycles = 0;
    fuCapacityStallCycles = 0;
    computeLatencyWaitCycles = 0;
    memoryServiceWaitCycles = 0;
    memoryIssueBackpressureCycles = 0;
    computeAndMemoryOutstandingWaitCycles = 0;
    schedulingBlockedCycles = 0;
    lockstepBlockedCycles = 0;
    idleCycles = 0;

    loadRawHazardCycles = 0;
    callWaitCycles = 0;
    thresholdBlockedCycles = 0;
    memoryBackpressureCycles = 0;
    allPortsStalledCycles = 0;
    portRetryCycles = 0;
    unissuedMemoryReqCycles = 0;
    reservationNonEmptyCycles = 0;
    readQueueNonEmptyCycles = 0;
    writeQueueNonEmptyCycles = 0;
    computeQueueNonEmptyCycles = 0;

    if (windowStatsEnable) {
        curWindow = WindowStats{};
        windowIndex = 0;
    }

    hw->hw_statistics->resetRun();
    hw->resetRuntimeFuStats();
    // hw->opcodes->reset_usage();

    tick();
}

void
LLVMInterface::debug(uint64_t flags)
{
    // Dump
    for (auto func_iter = functions.begin(); func_iter != functions.end();
         func_iter++) {
        // Function Level
        for (auto bb_iter = (*func_iter)->getBBList()->begin();
             bb_iter != (*func_iter)->getBBList()->end(); bb_iter++) {
            // Basic Block Level
            (*bb_iter)->dump();
            for (auto inst_iter = (*bb_iter)->Instructions()->begin();
                 inst_iter != (*bb_iter)->Instructions()->end(); inst_iter++) {
                // Instruction Level
                (*inst_iter)->dump();
            }
        }
    }
}

void
LLVMInterface::startup()
{
    /************************************************************************
     Initialize communications between gem5 interface and simulator
    ************************************************************************/
    comm->registerCompUnit(this);
}

void
LLVMInterface::finalize()
{
    if (windowStatsEnable && curWindow.cycles > 0) {
        emitWindowSummary();
    }

    rollUpCurrentInvocationIntoAggregate();
    // Simulation Times
    simStop = std::chrono::high_resolution_clock::now();
    simTotal = simStop - timeStart;
    printResults();
    functions.clear();
    values.clear();
    comm->finish();
}

void
LLVMInterface::emitSummaryLine() const
{
    uint64_t cycle_records = 0;
    uint64_t cycle_load_issue = 0;
    uint64_t cycle_load_commit = 0;
    uint64_t cycle_store_issue = 0;
    uint64_t cycle_store_commit = 0;
    uint64_t cycle_cmp_try = 0;
    uint64_t cycle_cmp_launch = 0;
    uint64_t cycle_cmp_commit = 0;
    uint64_t cycle_call_issue = 0;
    uint64_t cycle_call_commit = 0;
    uint64_t cycle_internal_load_completions = 0;

    if (hw->hw_statistics->use_cycle_tracking()) {
        const auto summary = hw->hw_statistics->summarize();
        cycle_records = summary.cyclesRecorded;
        cycle_load_issue = summary.totalLoadsIssued;
        cycle_load_commit = summary.totalLoadsCommitted;
        cycle_store_issue = summary.totalStoresIssued;
        cycle_store_commit = summary.totalStoresCommitted;
        cycle_cmp_try = summary.totalComputeAttempts;
        cycle_cmp_launch = summary.totalComputeLaunchesAccepted;
        cycle_cmp_commit = summary.totalComputeCommitted;
        cycle_call_issue = summary.totalCallsIssued;
        cycle_call_commit = summary.totalCallsCommitted;
        cycle_internal_load_completions = summary.totalInternalLoadCompletions;
    }

    std::cout << "SALAM_SUMMARY "
              << "name=" << name() << " runtime_cycles=" << cycle
              << " inst_issue=" << dynInstsIssued
              << " inst_commit=" << dynInstsCommitted
              << " load_issue=" << dynLoadsIssued
              << " load_commit=" << dynLoadsCommitted
              << " store_issue=" << dynStoresIssued
              << " store_commit=" << dynStoresCommitted
              << " cmp_try=" << dynComputeLaunchAttempts
              << " cmp_launch=" << dynComputeLaunched
              << " cmp_commit=" << dynComputeCommitted << " fu_denied="
              << (dynComputeLaunchAttempts - dynComputeLaunched)
              << " call_issue=" << dynCallsIssued
              << " call_commit=" << dynCallsCommitted
              << " cycle_records=" << cycle_records
              << " cycle_load_issue=" << cycle_load_issue
              << " cycle_load_commit=" << cycle_load_commit
              << " cycle_store_issue=" << cycle_store_issue
              << " cycle_store_commit=" << cycle_store_commit
              << " cycle_cmp_try=" << cycle_cmp_try
              << " cycle_cmp_launch=" << cycle_cmp_launch
              << " cycle_cmp_commit=" << cycle_cmp_commit
              << " cycle_call_issue=" << cycle_call_issue
              << " cycle_call_commit=" << cycle_call_commit
              << " cycle_internal_load_completions="
              << cycle_internal_load_completions
              << " internal_load_commit=" << cycle_internal_load_completions
              << " external_load_commit="
              << (cycle_load_commit - cycle_internal_load_completions)
              << " useful_compute=" << usefulComputeCycles
              << " useful_memory=" << usefulMemoryCycles
              << " useful_control=" << usefulControlCycles
              << " dep_stall=" << dependencyStallCycles
              << " fu_stall=" << fuCapacityStallCycles
              << " cmp_wait=" << computeLatencyWaitCycles
              << " mem_wait=" << memoryServiceWaitCycles
              << " mem_bp_wait=" << memoryIssueBackpressureCycles
              << " unissued_mem_req=" << unissuedMemoryReqCycles
              << " both_outstanding_wait="
              << computeAndMemoryOutstandingWaitCycles
              << " sched_blocked=" << schedulingBlockedCycles
              << " idle=" << idleCycles
              << " load_raw_hazard=" << loadRawHazardCycles
              << " call_wait=" << callWaitCycles
              << " threshold_blocked=" << thresholdBlockedCycles
              << " lockstep_wait=" << lockstepBlockedCycles
              << " all_ports_stalled=" << allPortsStalledCycles
              << " port_retry=" << portRetryCycles
              << " debug_stalls=" << stalls;

    if (hw->hw_statistics->use_cycle_tracking()) {
        const auto s = hw->hw_statistics->summarize();
        std::cout
            << " read_bp=" << s.readIssueBackpressureCycles
            << " write_bp=" << s.writeIssueBackpressureCycles
            << " read_retry=" << s.readRetryCycles
            << " write_retry=" << s.writeRetryCycles
            << " local_rd_issued_bytes="
            << s.totalIssuedMemBytes[(size_t)SalamMemTargetClass::Local]
                                    [(size_t)SalamAccessKind::Read]
            << " local_wr_issued_bytes="
            << s.totalIssuedMemBytes[(size_t)SalamMemTargetClass::Local]
                                    [(size_t)SalamAccessKind::Write]
            << " global_rd_issued_bytes="
            << s.totalIssuedMemBytes[(size_t)SalamMemTargetClass::Global]
                                    [(size_t)SalamAccessKind::Read]
            << " global_wr_issued_bytes="
            << s.totalIssuedMemBytes[(size_t)SalamMemTargetClass::Global]
                                    [(size_t)SalamAccessKind::Write]
            << " spm_rd_issued_bytes="
            << s.totalIssuedMemBytes[(size_t)SalamMemTargetClass::SPM]
                                    [(size_t)SalamAccessKind::Read]
            << " spm_wr_issued_bytes="
            << s.totalIssuedMemBytes[(size_t)SalamMemTargetClass::SPM]
                                    [(size_t)SalamAccessKind::Write]
            << " stream_rd_issued_bytes="
            << s.totalIssuedMemBytes[(size_t)SalamMemTargetClass::Stream]
                                    [(size_t)SalamAccessKind::Read]
            << " stream_wr_issued_bytes="
            << s.totalIssuedMemBytes[(size_t)SalamMemTargetClass::Stream]
                                    [(size_t)SalamAccessKind::Write]
            << " local_rd_accepted_bytes="
            << s.totalAcceptedMemBytes[(size_t)SalamMemTargetClass::Local]
                                      [(size_t)SalamAccessKind::Read]
            << " local_wr_accepted_bytes="
            << s.totalAcceptedMemBytes[(size_t)SalamMemTargetClass::Local]
                                      [(size_t)SalamAccessKind::Write]
            << " global_rd_accepted_bytes="
            << s.totalAcceptedMemBytes[(size_t)SalamMemTargetClass::Global]
                                      [(size_t)SalamAccessKind::Read]
            << " global_wr_accepted_bytes="
            << s.totalAcceptedMemBytes[(size_t)SalamMemTargetClass::Global]
                                      [(size_t)SalamAccessKind::Write]
            << " spm_rd_accepted_bytes="
            << s.totalAcceptedMemBytes[(size_t)SalamMemTargetClass::SPM]
                                      [(size_t)SalamAccessKind::Read]
            << " spm_wr_accepted_bytes="
            << s.totalAcceptedMemBytes[(size_t)SalamMemTargetClass::SPM]
                                      [(size_t)SalamAccessKind::Write]
            << " stream_rd_accepted_bytes="
            << s.totalAcceptedMemBytes[(size_t)SalamMemTargetClass::Stream]
                                      [(size_t)SalamAccessKind::Read]
            << " stream_wr_accepted_bytes="
            << s.totalAcceptedMemBytes[(size_t)SalamMemTargetClass::Stream]
                                      [(size_t)SalamAccessKind::Write];
    }

    std::cout << std::endl;

    if (invocationCount > 1) {
        std::cout << "SALAM_AGG_SUMMARY"
                  << " name=" << name() << " invocations=" << invocationCount
                  << " agg_cycles=" << aggCycles
                  << " agg_inst_issue=" << aggDynInstsIssued
                  << " agg_inst_commit=" << aggDynInstsCommitted
                  << " agg_load_issue=" << aggDynLoadsIssued
                  << " agg_load_commit=" << aggDynLoadsCommitted
                  << " agg_store_issue=" << aggDynStoresIssued
                  << " agg_store_commit=" << aggDynStoresCommitted
                  << " agg_cmp_try=" << aggDynComputeLaunchAttempts
                  << " agg_cmp_launch=" << aggDynComputeLaunched
                  << " agg_cmp_commit=" << aggDynComputeCommitted
                  << " agg_call_issue=" << aggDynCallsIssued
                  << " agg_call_commit=" << aggDynCallsCommitted
                  << " agg_useful_compute=" << aggUsefulComputeCycles
                  << " agg_useful_memory=" << aggUsefulMemoryCycles
                  << " agg_useful_control=" << aggUsefulControlCycles
                  << " agg_dep_stall=" << aggDependencyStallCycles
                  << " agg_fu_stall=" << aggFuCapacityStallCycles
                  << " agg_cmp_wait=" << aggComputeLatencyWaitCycles
                  << " agg_mem_wait=" << aggMemoryServiceWaitCycles
                  << " agg_mem_bp_wait=" << aggMemoryIssueBackpressureCycles
                  << " agg_unissued_mem_req=" << aggUnissuedMemoryReqCycles
                  << " agg_both_outstanding_wait="
                  << aggComputeAndMemoryOutstandingWaitCycles
                  << " agg_sched_blocked=" << aggSchedulingBlockedCycles
                  << " agg_idle=" << aggIdleCycles
                  << " agg_internal_load_commit=" << aggInternalLoadCompletions
                  << " agg_external_load_commit=" << aggExternalLoadCompletions
                  << std::endl;
    }
}

uint64_t
LLVMInterface::disjointCycleBreakdownTotal() const
{
    return usefulComputeCycles + usefulMemoryCycles + usefulControlCycles +
           dependencyStallCycles + fuCapacityStallCycles +
           computeLatencyWaitCycles + memoryServiceWaitCycles +
           memoryIssueBackpressureCycles +
           computeAndMemoryOutstandingWaitCycles + schedulingBlockedCycles +
           idleCycles;
}

void
LLVMInterface::printDisjointCycleBreakdown() const
{
    const uint64_t total_cycles = disjointCycleBreakdownTotal();

    const auto formatPercent = [](uint64_t value, uint64_t total) {
        std::ostringstream out;
        const double percent = (total > 0)
                                   ? (100.0 * static_cast<double>(value) /
                                      static_cast<double>(total))
                                   : 0.0;
        out << value << " (" << std::fixed << std::setprecision(4) << percent
            << "%)";
        return out.str();
    };

    const auto printLabelValue = [](const std::string &label,
                                    const std::string &value, int indent = 5) {
        std::cout << std::string(indent, ' ') << std::left << std::setw(40)
                  << (label + ":") << value << std::endl;
    };

    std::string dominant_label = "Idle";
    uint64_t dominant_value = idleCycles;

    const auto updateDominant = [&](const std::string &label, uint64_t value) {
        if (value > dominant_value) {
            dominant_label = label;
            dominant_value = value;
        }
    };

    updateDominant("Useful Compute", usefulComputeCycles);
    updateDominant("Useful Memory", usefulMemoryCycles);
    updateDominant("Useful Control", usefulControlCycles);
    updateDominant("Dependency Stall", dependencyStallCycles);
    updateDominant("FU Capacity Stall", fuCapacityStallCycles);
    updateDominant("Compute Latency Wait", computeLatencyWaitCycles);
    updateDominant("Memory Service Wait", memoryServiceWaitCycles);
    updateDominant("Memory Issue Backpressure", memoryIssueBackpressureCycles);
    updateDominant("Compute+Memory Outstanding Wait",
                   computeAndMemoryOutstandingWaitCycles);
    updateDominant("Scheduling Blocked", schedulingBlockedCycles);

    std::cout << "  Disjoint Cycle Breakdown" << std::endl;
    std::cout << "  ------------------------" << std::endl;
    printLabelValue("Dominant Category",
                    dominant_label + ", " +
                        formatPercent(dominant_value, total_cycles));
    printLabelValue("Useful Compute Cycles",
                    formatPercent(usefulComputeCycles, total_cycles));
    printLabelValue("Useful Memory Cycles",
                    formatPercent(usefulMemoryCycles, total_cycles));
    printLabelValue("Useful Control Cycles",
                    formatPercent(usefulControlCycles, total_cycles));
    printLabelValue("Dependency Stall Cycles",
                    formatPercent(dependencyStallCycles, total_cycles));
    printLabelValue("FU Capacity Stall Cycles",
                    formatPercent(fuCapacityStallCycles, total_cycles));
    printLabelValue("Compute Latency Wait Cycles",
                    formatPercent(computeLatencyWaitCycles, total_cycles));
    printLabelValue("Memory Service Wait Cycles",
                    formatPercent(memoryServiceWaitCycles, total_cycles));
    printLabelValue(
        "Memory Issue Backpressure Cycles",
        formatPercent(memoryIssueBackpressureCycles, total_cycles));
    printLabelValue(
        "Compute+Memory Outstanding Wait Cycles",
        formatPercent(computeAndMemoryOutstandingWaitCycles, total_cycles));
    printLabelValue("Scheduling Blocked Cycles",
                    formatPercent(schedulingBlockedCycles, total_cycles));
    printLabelValue("Idle Cycles", formatPercent(idleCycles, total_cycles));
    printLabelValue("Total Cycles Accounted", std::to_string(total_cycles));
    std::cout << std::endl;

    std::cout << "  Overlapping Diagnostics" << std::endl;
    std::cout << "  -----------------------" << std::endl;
    printLabelValue("Load RAW Hazard Cycles",
                    std::to_string(loadRawHazardCycles));
    printLabelValue("Call Wait Cycles", std::to_string(callWaitCycles));
    printLabelValue("Threshold Blocked Cycles",
                    std::to_string(thresholdBlockedCycles));
    printLabelValue("Lockstep Blocked Cycles",
                    std::to_string(lockstepBlockedCycles));
    printLabelValue("Memory Backpressure Cycles",
                    std::to_string(memoryBackpressureCycles));
    printLabelValue("All Ports Stalled Cycles",
                    std::to_string(allPortsStalledCycles));
    printLabelValue("Port Retry Cycles", std::to_string(portRetryCycles));
    printLabelValue("Reservation Nonempty Cycles",
                    std::to_string(reservationNonEmptyCycles));
    printLabelValue("Read Queue Nonempty Cycles",
                    std::to_string(readQueueNonEmptyCycles));
    printLabelValue("Write Queue Nonempty Cycles",
                    std::to_string(writeQueueNonEmptyCycles));
    printLabelValue("Compute Queue Nonempty Cycles",
                    std::to_string(computeQueueNonEmptyCycles));
    std::cout << std::endl;
}

void
LLVMInterface::printResults()
{
    std::map<uint64_t, uint64_t> total_reads;
    std::map<uint64_t, uint64_t> total_writes;

    for (auto value : values) {
        if (value->isInstruction()) {
            if (value->getReg()) {
                total_reads[value->getOpode()] += value->getReg()->getReads();
                total_writes[value->getOpode()] +=
                    value->getReg()->getWrites();
            }
        }
    }

    double adder_area =
        (hw->opcodes->get_usage(13) + hw->opcodes->get_usage(20) +
         hw->opcodes->get_usage(15)) *
        1.794430e+02;
    double adder_reads = total_reads[13] + total_reads[15];
    double adder_writes = total_writes[13] + total_writes[15];
    double adder_power_static = adder_reads * 2.380803e-03;
    double adder_power_dynamic = adder_writes * (8.115300e-03 + 6.162853e-03);

    double bitwise_area =
        (hw->opcodes->get_usage(29) + hw->opcodes->get_usage(30) +
         hw->opcodes->get_usage(25) + hw->opcodes->get_usage(26) +
         hw->opcodes->get_usage(27) + hw->opcodes->get_usage(28)) *
        5.036996e+01;
    double bitwise_reads = total_reads[25] + total_reads[26] +
                           total_reads[27] + total_reads[28] +
                           total_reads[29] + total_reads[30];
    double bitwise_writes = total_writes[25] + total_writes[26] +
                            total_writes[27] + total_writes[28] +
                            total_writes[29] + total_writes[30];
    double bitwise_power_static = bitwise_reads * 6.111633e-04;
    double bitwise_power_dynamic =
        bitwise_writes * (1.680942e-03 + 1.322420e-03);

    double multiplier_area =
        (hw->opcodes->get_usage(17) + hw->opcodes->get_usage(19) +
         hw->opcodes->get_usage(20)) *
        4.595000e+03;
    double multiplier_reads =
        total_reads[17] + total_reads[19] + total_reads[20];
    double multiplier_writes =
        total_writes[17] + total_writes[19] + total_writes[20];
    double multiplier_power_static = multiplier_reads * 4.817683e-02;
    double multiplier_power_dynamic =
        multiplier_writes * (5.725752e-01 + 8.662890e-01);

    double register_area = hw->opcodes->get_usage(34) * 32 * 5.981433e+00;
    double register_reads = total_reads[34] * 32;
    double register_writes = total_writes[34] * 32;
    double register_power_static = register_reads * 7.395312e-05;
    double register_power_dynamic =
        register_writes * (1.322600e-03 + 1.792126e-04);

    double total_area =
        adder_area + bitwise_area + multiplier_area + register_area;
    double total_power_static = adder_power_static + bitwise_power_static +
                                multiplier_power_static +
                                register_power_static;
    double total_power_dynamic = adder_power_dynamic + bitwise_power_dynamic +
                                 multiplier_power_dynamic +
                                 register_power_dynamic;

    const double cycle_time_ns =
        static_cast<double>(clockPeriod()) / gem5::sim_clock::as_float::ns;
    const double frequency_ghz =
        (cycle_time_ns > 0.0) ? (1.0 / cycle_time_ns) : 0.0;
    const double runtime_us =
        (static_cast<double>(clockPeriod()) * static_cast<double>(cycle)) /
        gem5::sim_clock::as_float::us;

    const uint64_t compute_launch_denied =
        dynComputeLaunchAttempts - dynComputeLaunched;

    const auto formatDouble = [](double value, int precision = 4) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision) << value;
        return out.str();
    };

    const auto formatDuration = [](auto duration_value) {
        using namespace std::chrono;

        auto duration_us = duration_cast<microseconds>(duration_value);
        auto duration_hours = duration_cast<hours>(duration_us);
        duration_us -= duration_cast<microseconds>(duration_hours);

        auto duration_minutes = duration_cast<minutes>(duration_us);
        duration_us -= duration_cast<microseconds>(duration_minutes);

        auto duration_seconds = duration_cast<seconds>(duration_us);
        duration_us -= duration_cast<microseconds>(duration_seconds);

        auto duration_milliseconds = duration_cast<milliseconds>(duration_us);
        duration_us -= duration_cast<microseconds>(duration_milliseconds);

        std::ostringstream out;
        out << duration_hours.count() << "h " << duration_minutes.count()
            << "m " << duration_seconds.count() << "s "
            << duration_milliseconds.count() << "ms";
        if (duration_us.count() > 0) {
            out << " " << duration_us.count() << "us";
        }
        return out.str();
    };

    const auto printDivider = []() {
        std::cout << "======================================================"
                  << std::endl;
    };

    const auto printSection = [](const std::string &title) {
        std::cout << "  " << title << std::endl;
        std::cout << "  " << std::string(title.size(), '-') << std::endl;
    };

    const auto printLabelValue = [](const std::string &label,
                                    const std::string &value, int indent = 5) {
        std::cout << std::string(indent, ' ') << std::left << std::setw(40)
                  << (label + ":") << value << std::endl;
    };

    printDivider();
    std::cout << name() << std::endl;
    printDivider();

    printSection("Simulated Accelerator Metrics");
    printLabelValue("Compute Clock", formatDouble(frequency_ghz) + " GHz");
    printLabelValue("Runtime", std::to_string(cycle) + " cycles (" +
                                   formatDouble(runtime_us) + " us)");
    printLabelValue("Dynamic Instructions",
                    std::to_string(dynInstsIssued) + " issued / " +
                        std::to_string(dynInstsCommitted) + " committed");
    printLabelValue("Loads", std::to_string(dynLoadsIssued) + " issued / " +
                                 std::to_string(dynLoadsCommitted) +
                                 " committed");
    printLabelValue("Stores", std::to_string(dynStoresIssued) + " issued / " +
                                  std::to_string(dynStoresCommitted) +
                                  " committed");
    printLabelValue("Compute",
                    std::to_string(dynComputeLaunchAttempts) + " attempts / " +
                        std::to_string(dynComputeLaunched) + " accepted / " +
                        std::to_string(compute_launch_denied) + " denied / " +
                        std::to_string(dynComputeCommitted) + " committed");
    printLabelValue("Calls", std::to_string(dynCallsIssued) + " issued / " +
                                 std::to_string(dynCallsCommitted) +
                                 " committed");
    std::cout << std::endl;

    printDisjointCycleBreakdown();

    if (hw->hw_statistics->use_cycle_tracking()) {
        const auto summary = hw->hw_statistics->summarize();

        printSection("Cycle Summary");

        printQueueSummary(summary);

        std::cout << "  Event Totals" << std::endl;
        printLabelValue("Cycles Recorded",
                        std::to_string(summary.cyclesRecorded));
        printLabelValue("Internal Load Completions",
                        std::to_string(summary.totalInternalLoadCompletions));
        printLabelValue(
            "Loads", std::to_string(summary.totalLoadsIssued) + " issued / " +
                         std::to_string(summary.totalLoadsCommitted) +
                         " committed");
        printLabelValue(
            "Stores",
            std::to_string(summary.totalStoresIssued) + " issued / " +
                std::to_string(summary.totalStoresCommitted) + " committed");
        printLabelValue(
            "Compute",
            std::to_string(summary.totalComputeAttempts) + " attempts / " +
                std::to_string(summary.totalComputeLaunchesAccepted) +
                " accepted / " +
                std::to_string(summary.totalComputeCommitted) + " committed");
        printLabelValue(
            "Calls", std::to_string(summary.totalCallsIssued) + " issued / " +
                         std::to_string(summary.totalCallsCommitted) +
                         " committed");
        std::cout << std::endl;

        std::cout << "  Cycle Diagnostics" << std::endl;
        printLabelValue("Load RAW Hazard Cycles",
                        std::to_string(summary.loadRawHazardCycles));
        printLabelValue("FU Capacity Deny Cycles",
                        std::to_string(summary.fuCapacityDenyCycles));
        printLabelValue("Call Wait Cycles",
                        std::to_string(summary.callWaitCycles));
        printLabelValue("Threshold Blocked Cycles",
                        std::to_string(summary.thresholdBlockedCycles));
        printLabelValue("Lockstep Blocked Cycles",
                        std::to_string(summary.lockstepBlockedCycles));
        printLabelValue("Issue Backpressure Cycles",
                        std::to_string(summary.issueBackpressureCycles));
        printLabelValue("All Ports Stalled Cycles",
                        std::to_string(summary.allPortsStalledCycles));
        printLabelValue("Port Retry Cycles",
                        std::to_string(summary.portRetryCycles));
        printLabelValue("Outstanding Memory Cycles",
                        std::to_string(summary.outstandingMemoryCycles));
        printLabelValue("Outstanding Compute Cycles",
                        std::to_string(summary.outstandingComputeCycles));
        printLabelValue("Ready Memory Cycles",
                        std::to_string(summary.readyMemoryCycles));
        printLabelValue("Ready Compute Cycles",
                        std::to_string(summary.readyComputeCycles));
        printLabelValue("Reservation Nonempty Cycles",
                        std::to_string(summary.reservationNonEmptyCycles));
        printLabelValue("Read Queue Nonempty Cycles",
                        std::to_string(summary.readQueueNonEmptyCycles));
        printLabelValue("Write Queue Nonempty Cycles",
                        std::to_string(summary.writeQueueNonEmptyCycles));
        printLabelValue("Compute Queue Nonempty Cycles",
                        std::to_string(summary.computeQueueNonEmptyCycles));
        std::cout << std::endl;

        printTrafficSummary(summary);
        printOverlapSummary(summary);
        printResourceSummary(summary);
    }

    {
        printSection("Derived Metrics");
        const double ipc =
            cycle > 0 ? static_cast<double>(dynInstsCommitted) / cycle : 0.0;
        const double computeToMemRatio =
            (dynLoadsCommitted + dynStoresCommitted) > 0
                ? static_cast<double>(dynComputeCommitted) /
                      static_cast<double>(dynLoadsCommitted +
                                          dynStoresCommitted)
                : 0.0;
        const uint64_t computeSensitiveCycles = usefulComputeCycles +
                                                computeLatencyWaitCycles +
                                                fuCapacityStallCycles;
        const uint64_t memorySensitiveCycles =
            usefulMemoryCycles + memoryServiceWaitCycles;
        const uint64_t fabricSensitiveCycles = memoryIssueBackpressureCycles;
        const uint64_t fixedOrSchedulerCycles =
            dependencyStallCycles + schedulingBlockedCycles + idleCycles;
        const uint64_t totalCycles = disjointCycleBreakdownTotal();
        const auto frac = [&](uint64_t v) {
            return formatDouble(totalCycles > 0
                                    ? static_cast<double>(v) /
                                          static_cast<double>(totalCycles)
                                    : 0.0);
        };
        printLabelValue("IPC", formatDouble(ipc));
        printLabelValue("Compute-to-Memory Ratio",
                        formatDouble(computeToMemRatio));
        printLabelValue("Compute-Sensitive Fraction",
                        frac(computeSensitiveCycles));
        printLabelValue("Memory-Sensitive Fraction",
                        frac(memorySensitiveCycles));
        printLabelValue("Fabric-Sensitive Fraction",
                        frac(fabricSensitiveCycles));
        printLabelValue("Fixed/Scheduler Fraction",
                        frac(fixedOrSchedulerCycles));

        std::cout << std::endl;

        if (invocationCount > 1) {
            printSection("Aggregate Metrics (All Invocations)");
            printLabelValue("Invocations", std::to_string(invocationCount));
            printLabelValue("Aggregate Cycles", std::to_string(aggCycles));
            printLabelValue("Aggregate Instructions",
                            std::to_string(aggDynInstsIssued) + " issued / " +
                                std::to_string(aggDynInstsCommitted) +
                                " committed");
            const double aggIpc =
                aggCycles > 0
                    ? static_cast<double>(aggDynInstsCommitted) / aggCycles
                    : 0.0;
            printLabelValue("Aggregate IPC", formatDouble(aggIpc));
            const uint64_t aggTotalCycles =
                aggUsefulComputeCycles + aggUsefulMemoryCycles +
                aggUsefulControlCycles + aggDependencyStallCycles +
                aggFuCapacityStallCycles + aggComputeLatencyWaitCycles +
                aggMemoryServiceWaitCycles + aggMemoryIssueBackpressureCycles +
                aggComputeAndMemoryOutstandingWaitCycles +
                aggSchedulingBlockedCycles + aggIdleCycles;
            const auto aggFrac = [&](uint64_t v) {
                return formatDouble(
                    aggTotalCycles > 0
                        ? static_cast<double>(v) /
                              static_cast<double>(aggTotalCycles)
                        : 0.0);
            };
            printLabelValue("Agg Compute-Sensitive Fraction",
                            aggFrac(aggUsefulComputeCycles +
                                    aggComputeLatencyWaitCycles +
                                    aggFuCapacityStallCycles));
            printLabelValue(
                "Agg Memory-Sensitive Fraction",
                aggFrac(aggUsefulMemoryCycles + aggMemoryServiceWaitCycles));
            printLabelValue("Agg Fabric-Sensitive Fraction",
                            aggFrac(aggMemoryIssueBackpressureCycles));
            std::cout << std::endl;
        }
    }

    printSection("Power / Area (Current Approximation)");
    printLabelValue("Total Area", formatDouble(total_area));
    printLabelValue("Total Power Static", formatDouble(total_power_static));
    printLabelValue("Total Power Dynamic", formatDouble(total_power_dynamic));

    std::cout << std::endl;

    printSection("Host Simulator Overhead");
    printLabelValue("Host Setup Time", formatDuration(setupTime));
    printLabelValue("Host Simulation Time (Total)", formatDuration(simTotal));
    printLabelValue("Host Simulation Time (Active)", formatDuration(simTime));
    printLabelValue("Host Queue Processing Time",
                    formatDuration(queueProcessTime));
    printLabelValue("Host Scheduling Time", formatDuration(schedulingTime));
    printLabelValue("Host Computation Time", formatDuration(computeTime));
    std::cout << std::endl;

    emitSummaryLine();
}

void
LLVMInterface::printQueueSummary(const HW_Stats_Summary &summary) const
{
    const auto formatDouble = [](double value, int precision = 4) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision) << value;
        return out.str();
    };

    const auto printLabelValue = [](const std::string &label,
                                    const std::string &value, int indent = 5) {
        std::cout << std::string(indent, ' ') << std::left << std::setw(40)
                  << (label + ":") << value << std::endl;
    };

    const double n = static_cast<double>(summary.cyclesRecorded);

    std::cout << "  Queue Depths (End-of-Cycle)" << std::endl;
    printLabelValue("Avg Reservation End Depth",
                    formatDouble(summary.reservationDepthEndSum / n));
    printLabelValue("Avg Read End Depth",
                    formatDouble(summary.readQueueDepthEndSum / n));
    printLabelValue("Avg Write End Depth",
                    formatDouble(summary.writeQueueDepthEndSum / n));
    printLabelValue("Avg Compute End Depth",
                    formatDouble(summary.computeQueueDepthEndSum / n));
    std::cout << std::endl;

    std::cout << "  Queue Depths (Intra-Cycle Peak)" << std::endl;
    printLabelValue("Avg Reservation Peak Depth",
                    formatDouble(summary.reservationDepthPeakSum / n));
    printLabelValue("Avg Read Peak Depth",
                    formatDouble(summary.readQueueDepthPeakSum / n));
    printLabelValue("Avg Write Peak Depth",
                    formatDouble(summary.writeQueueDepthPeakSum / n));
    printLabelValue("Avg Compute Peak Depth",
                    formatDouble(summary.computeQueueDepthPeakSum / n));
    printLabelValue("Max Reservation Peak",
                    std::to_string(summary.maxReservationDepthPeak));
    printLabelValue("Max Read Peak",
                    std::to_string(summary.maxReadQueueDepthPeak));
    printLabelValue("Max Write Peak",
                    std::to_string(summary.maxWriteQueueDepthPeak));
    printLabelValue("Max Compute Peak",
                    std::to_string(summary.maxComputeQueueDepthPeak));
    std::cout << std::endl;
}

void
LLVMInterface::printTrafficSummary(const HW_Stats_Summary &summary) const
{
    const auto printLabelValue = [](const std::string &label,
                                    const std::string &value, int indent = 5) {
        std::cout << std::string(indent, ' ') << std::left << std::setw(40)
                  << (label + ":") << value << std::endl;
    };

    static const char *targetNames[] = {"Local", "Global", "SPM", "Stream"};

    std::cout << "  Memory Traffic (issued vs accepted)" << std::endl;
    std::cout << "  -----------------------------------" << std::endl;
    for (size_t t = 0; t < kNumTargetClasses; ++t) {
        const uint64_t rd_ops_iss =
            summary.totalIssuedMemOps[t][(size_t)SalamAccessKind::Read];
        const uint64_t wr_ops_iss =
            summary.totalIssuedMemOps[t][(size_t)SalamAccessKind::Write];
        const uint64_t rd_bytes_iss =
            summary.totalIssuedMemBytes[t][(size_t)SalamAccessKind::Read];
        const uint64_t wr_bytes_iss =
            summary.totalIssuedMemBytes[t][(size_t)SalamAccessKind::Write];
        const uint64_t rd_ops_acc =
            summary.totalAcceptedMemOps[t][(size_t)SalamAccessKind::Read];
        const uint64_t wr_ops_acc =
            summary.totalAcceptedMemOps[t][(size_t)SalamAccessKind::Write];
        const uint64_t rd_bytes_acc =
            summary.totalAcceptedMemBytes[t][(size_t)SalamAccessKind::Read];
        const uint64_t wr_bytes_acc =
            summary.totalAcceptedMemBytes[t][(size_t)SalamAccessKind::Write];
        if (rd_ops_iss == 0 && wr_ops_iss == 0) {
            continue;
        }
        std::string prefix = targetNames[t];
        printLabelValue(prefix + " Issued Read Ops",
                        std::to_string(rd_ops_iss));
        printLabelValue(prefix + " Issued Read Bytes",
                        std::to_string(rd_bytes_iss));
        printLabelValue(prefix + " Issued Write Ops",
                        std::to_string(wr_ops_iss));
        printLabelValue(prefix + " Issued Write Bytes",
                        std::to_string(wr_bytes_iss));
        printLabelValue(prefix + " Accepted Read Ops",
                        std::to_string(rd_ops_acc));
        printLabelValue(prefix + " Accepted Read Bytes",
                        std::to_string(rd_bytes_acc));
        printLabelValue(prefix + " Accepted Write Ops",
                        std::to_string(wr_ops_acc));
        printLabelValue(prefix + " Accepted Write Bytes",
                        std::to_string(wr_bytes_acc));
    }
    printLabelValue("Read Issue Backpressure Cycles",
                    std::to_string(summary.readIssueBackpressureCycles));
    printLabelValue("Write Issue Backpressure Cycles",
                    std::to_string(summary.writeIssueBackpressureCycles));
    printLabelValue("Read Retry Cycles",
                    std::to_string(summary.readRetryCycles));
    printLabelValue("Write Retry Cycles",
                    std::to_string(summary.writeRetryCycles));
    std::cout << std::endl;
}

void
LLVMInterface::printOverlapSummary(const HW_Stats_Summary &summary) const
{
    const auto printLabelValue = [](const std::string &label,
                                    const std::string &value, int indent = 5) {
        std::cout << std::string(indent, ' ') << std::left << std::setw(40)
                  << (label + ":") << value << std::endl;
    };

    std::cout << "  Overlap / Concurrency" << std::endl;
    std::cout << "  ---------------------" << std::endl;
    printLabelValue("Compute & Memory Outstanding Cycles",
                    std::to_string(summary.computeAndMemoryOutstandingCycles));
    printLabelValue("Compute & Memory Ready Cycles",
                    std::to_string(summary.computeAndMemoryReadyCycles));
    std::cout << std::endl;
}

void
LLVMInterface::printResourceSummary(const HW_Stats_Summary &summary) const
{
    const auto formatDouble = [](double value, int precision = 4) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision) << value;
        return out.str();
    };

    const auto printLabelValue = [](const std::string &label,
                                    const std::string &value, int indent = 5) {
        std::cout << std::string(indent, ' ') << std::left << std::setw(40)
                  << (label + ":") << value << std::endl;
    };

    static const char *fuNames[] = {
        "IntAdder",       "IntMultiplier", "IntShifter",     "IntBitwise",
        "FpSpAdder",      "FpDpAdder",     "FpSpMultiplier", "FpSpDivider",
        "FpDpMultiplier", "FpDpDivider",   "Register"};

    const double n = static_cast<double>(summary.cyclesRecorded);
    bool anyFuActivity = false;
    for (size_t f = 0; f < kNumFuClasses; ++f) {
        if (summary.fuAccepted[f] > 0 || summary.fuDenied[f] > 0) {
            anyFuActivity = true;
            break;
        }
    }

    if (!anyFuActivity) {
        return;
    }

    std::cout << "  Functional Unit Resources" << std::endl;
    std::cout << "  -------------------------" << std::endl;
    for (size_t f = 0; f < kNumFuClasses; ++f) {
        if (summary.fuAccepted[f] == 0 && summary.fuDenied[f] == 0) {
            continue;
        }
        std::string prefix = fuNames[f];
        printLabelValue(prefix + " Accepted",
                        std::to_string(summary.fuAccepted[f]));
        printLabelValue(prefix + " Denied",
                        std::to_string(summary.fuDenied[f]));
        printLabelValue(
            prefix + " Avg Busy Slots",
            formatDouble(n > 0 ? summary.fuBusySlotSum[f] / n : 0.0));
        printLabelValue(prefix + " Peak Busy Slots",
                        std::to_string(summary.fuPeakBusySlots[f]));
        printLabelValue(prefix + " Active Cycles",
                        std::to_string(summary.fuActiveCycles[f]));
    }
    std::cout << std::endl;
}

void
LLVMInterface::dumpQueues()
{
    // if (DTRACE(Trace))
    //     DPRINTF(Runtime, "Trace: %s \n", __PRETTY_FUNCTION__);
    // std::cout << "*****************************************************\n"
    //           << "Compute Queue\n"
    //           << "*****************************************************\n";
    // for (auto compute : computeQueue) {
    //     std::cout << compute->_LLVMLine << std::endl;
    // }
    // std::cout << "*****************************************************\n"
    //           << "Read Queue\n"
    //           << "*****************************************************\n";
    // for (auto read : readQueue) {
    //     std::cout << read->_LLVMLine << std::endl;
    // }
    // std::cout << "*****************************************************\n"
    //           << "Write Queue\n"
    //           << "*****************************************************\n";
    // for (auto write : writeQueue) {
    //     std::cout << write->_LLVMLine << std::endl;
    // }
    // std::cout << "*****************************************************\n"
    //           << "Reservation Queue\n"
    //           << "*****************************************************\n";
    // for (auto reserved : reservation) {
    //     std::cout << reserved->_LLVMLine << std::endl;
    // }
    // std::cout << "*****************************************************\n"
    //           << "End of queue dump\n"
    //           << "*****************************************************\n";
}

void
LLVMInterface::launchFunction(std::shared_ptr<SALAM::Function> callee,
                              std::shared_ptr<SALAM::Instruction> caller)
{
    // Add the callee to our list of active functions
    activeFunctions.push_back(ActiveFunction(this, callee, caller));
    activeFunctions.back().launch();
}

void
LLVMInterface::launchTopFunction()
{
    for (auto it = functions.begin(); it != functions.end(); it++) {
        if ((*it)->isTop()) {
            // Launch the top level function
            launchFunction((*it), nullptr);
            return;
        }
    }
    // Fallback if no function was marked as the top-level
    panic("No top-level function (set top_name param for LLVMInterface)\n");
}

void
LLVMInterface::ActiveFunction::launch()
{
    if (dbg) {
        DPRINTFS(LLVMInterface, owner, "Launching Function: %s\n",
                 func->getIRStub());
    }
    // Fetch the arguments
    std::vector<std::shared_ptr<SALAM::Value>> funcArgs =
        *(func->getArguments());
    if (func->isTop()) {
        // We need to fetch argument values from the memory mapped registers
        if (dbg) {
            DPRINTFS(LLVMInterface, owner, "Connecting CommInterface\n");
        }
        CommInterface *comm = owner->getCommInterface();
        if (dbg) {
            DPRINTFS(LLVMInterface, owner, "Connecting HWInterface\n");
        }
        hw = owner->getHWInterface();

        unsigned argOffset = 0;
        for (auto arg : funcArgs) {
            uint64_t argSizeInBytes = arg->getSizeInBytes();
            uint64_t regValue = comm->getGlobalVar(argOffset, argSizeInBytes);
            arg->setRegisterValue(regValue);
            argOffset += argSizeInBytes;
        }
    } else {
        // We need to fetch argument values from the calling function
        std::vector<SALAM::Operand> callerArgs = *caller->getOperands();
        if (funcArgs.size() != callerArgs.size()) {
            panic("Function expects %d args. Got %d args.", funcArgs.size(),
                  callerArgs.size());
        }
        for (auto i = 0; i < callerArgs.size(); i++) {
            funcArgs.at(i)->setRegisterValue(callerArgs.at(i).getOpRegister());
        }
    }
    func->addInstance();
    // Schedule the first BB
    scheduleBB(func->entry());
}

std::shared_ptr<SALAM::Instruction>
LLVMInterface::createInstruction(llvm::Instruction *inst, uint64_t id)
{
    uint64_t OpCode = inst->Instruction::getOpcode();
    hw->opcodes->update_usage(OpCode);

    uint64_t functional_unit = 0;
    for (auto hw_inst : hw->inst_config->inst_list) {
        if (OpCode == hw_inst->get_opcode_num()) {
            functional_unit = hw_inst->get_functional_unit();
            // Static IR construction determines opcode/FU mapping only
            // Runtime FU capacity comes from configured hardware limits,
            // not from the number of static IR nodes using that FU
            break;
        }
    }

    std::shared_ptr<SALAM::Instruction> created;

    switch (OpCode) {
        case llvm::Instruction::Ret:
            created = SALAM::createRetInst(id, this, debug(), OpCode,
                                           hw->cycle_counts->ret_inst,
                                           functional_unit);
            break;
        case llvm::Instruction::Br:
            created = SALAM::createBrInst(id, this, debug(), OpCode,
                                          hw->cycle_counts->br_inst,
                                          functional_unit);
            break;
        case llvm::Instruction::Switch:
            created = SALAM::createSwitchInst(id, this, debug(), OpCode,
                                              hw->cycle_counts->switch_inst,
                                              functional_unit);
            break;
        case llvm::Instruction::Add:
            created = SALAM::createAddInst(id, this, debug(), OpCode,
                                           hw->cycle_counts->add_inst,
                                           functional_unit);
            break;
        case llvm::Instruction::FAdd:
            created = SALAM::createFAddInst(id, this, debug(), OpCode,
                                            hw->cycle_counts->fadd_inst,
                                            functional_unit);
            break;
        case llvm::Instruction::Sub:
            created = SALAM::createSubInst(id, this, debug(), OpCode,
                                           hw->cycle_counts->sub_inst,
                                           functional_unit);
            break;
        case llvm::Instruction::FSub:
            created = SALAM::createFSubInst(id, this, debug(), OpCode,
                                            hw->cycle_counts->fsub_inst,
                                            functional_unit);
            break;
        case llvm::Instruction::Mul:
            created = SALAM::createMulInst(id, this, debug(), OpCode,
                                           hw->cycle_counts->mul_inst,
                                           functional_unit);
            break;
        case llvm::Instruction::FMul:
            created = SALAM::createFMulInst(id, this, debug(), OpCode,
                                            hw->cycle_counts->fmul_inst,
                                            functional_unit);
            break;
        case llvm::Instruction::UDiv:
            created = SALAM::createUDivInst(id, this, debug(), OpCode,
                                            hw->cycle_counts->udiv_inst,
                                            functional_unit);
            break;
        case llvm::Instruction::SDiv:
            created = SALAM::createSDivInst(id, this, debug(), OpCode,
                                            hw->cycle_counts->sdiv_inst,
                                            functional_unit);
            break;
        case llvm::Instruction::FDiv:
            created = SALAM::createFDivInst(id, this, debug(), OpCode,
                                            hw->cycle_counts->fdiv_inst,
                                            functional_unit);
            break;
        case llvm::Instruction::URem:
            created = SALAM::createURemInst(id, this, debug(), OpCode,
                                            hw->cycle_counts->urem_inst,
                                            functional_unit);
            break;
        case llvm::Instruction::SRem:
            created = SALAM::createSRemInst(id, this, debug(), OpCode,
                                            hw->cycle_counts->srem_inst,
                                            functional_unit);
            break;
        case llvm::Instruction::FRem:
            created = SALAM::createFRemInst(id, this, debug(), OpCode,
                                            hw->cycle_counts->frem_inst,
                                            functional_unit);
            break;
        case llvm::Instruction::Shl:
            created = SALAM::createShlInst(id, this, debug(), OpCode,
                                           hw->cycle_counts->shl_inst,
                                           functional_unit);
            break;
        case llvm::Instruction::LShr:
            created = SALAM::createLShrInst(id, this, debug(), OpCode,
                                            hw->cycle_counts->lshr_inst,
                                            functional_unit);
            break;
        case llvm::Instruction::AShr:
            created = SALAM::createAShrInst(id, this, debug(), OpCode,
                                            hw->cycle_counts->ashr_inst,
                                            functional_unit);
            break;
        case llvm::Instruction::And:
            created = SALAM::createAndInst(id, this, debug(), OpCode,
                                           hw->cycle_counts->and_inst,
                                           functional_unit);
            break;
        case llvm::Instruction::Or:
            created = SALAM::createOrInst(id, this, debug(), OpCode,
                                          hw->cycle_counts->or_inst,
                                          functional_unit);
            break;
        case llvm::Instruction::Xor:
            created = SALAM::createXorInst(id, this, debug(), OpCode,
                                           hw->cycle_counts->xor_inst,
                                           functional_unit);
            break;
        case llvm::Instruction::Load:
            created = SALAM::createLoadInst(id, this, debug(), OpCode,
                                            hw->cycle_counts->load_inst,
                                            functional_unit);
            break;
        case llvm::Instruction::Store:
            created = SALAM::createStoreInst(id, this, debug(), OpCode,
                                             hw->cycle_counts->store_inst,
                                             functional_unit);
            break;
        case llvm::Instruction::GetElementPtr:
            created = SALAM::createGetElementPtrInst(
                id, this, debug(), OpCode, hw->cycle_counts->gep_inst,
                functional_unit);
            break;
        case llvm::Instruction::Trunc:
            created = SALAM::createTruncInst(id, this, debug(), OpCode,
                                             hw->cycle_counts->trunc_inst,
                                             functional_unit);
            break;
        case llvm::Instruction::ZExt:
            created = SALAM::createZExtInst(id, this, debug(), OpCode,
                                            hw->cycle_counts->zext_inst,
                                            functional_unit);
            break;
        case llvm::Instruction::SExt:
            created = SALAM::createSExtInst(id, this, debug(), OpCode,
                                            hw->cycle_counts->sext_inst,
                                            functional_unit);
            break;
        case llvm::Instruction::FPToUI:
            created = SALAM::createFPToUIInst(id, this, debug(), OpCode,
                                              hw->cycle_counts->fptoui_inst,
                                              functional_unit);
            break;
        case llvm::Instruction::FPToSI:
            created = SALAM::createFPToSIInst(id, this, debug(), OpCode,
                                              hw->cycle_counts->fptosi_inst,
                                              functional_unit);
            break;
        case llvm::Instruction::UIToFP:
            created = SALAM::createUIToFPInst(id, this, debug(), OpCode,
                                              hw->cycle_counts->uitofp_inst,
                                              functional_unit);
            break;
        case llvm::Instruction::SIToFP:
            created = SALAM::createSIToFPInst(id, this, debug(), OpCode,
                                              hw->cycle_counts->sitofp_inst,
                                              functional_unit);
            break;
        case llvm::Instruction::FPTrunc:
            created = SALAM::createFPTruncInst(id, this, debug(), OpCode,
                                               hw->cycle_counts->fptrunc_inst,
                                               functional_unit);
            break;
        case llvm::Instruction::FPExt:
            created = SALAM::createFPExtInst(id, this, debug(), OpCode,
                                             hw->cycle_counts->fpext_inst,
                                             functional_unit);
            break;
        case llvm::Instruction::PtrToInt:
            created = SALAM::createPtrToIntInst(
                id, this, debug(), OpCode, hw->cycle_counts->ptrtoint_inst,
                functional_unit);
            break;
        case llvm::Instruction::IntToPtr:
            created = SALAM::createIntToPtrInst(
                id, this, debug(), OpCode, hw->cycle_counts->inttoptr_inst,
                functional_unit);
            break;
        case llvm::Instruction::ICmp:
            created = SALAM::createICmpInst(id, this, debug(), OpCode,
                                            hw->cycle_counts->icmp_inst,
                                            functional_unit);
            break;
        case llvm::Instruction::FCmp:
            created = SALAM::createFCmpInst(id, this, debug(), OpCode,
                                            hw->cycle_counts->fcmp_inst,
                                            functional_unit);
            break;
        case llvm::Instruction::PHI:
            created = SALAM::createPHIInst(id, this, debug(), OpCode,
                                           hw->cycle_counts->phi_inst,
                                           functional_unit);
            break;
        case llvm::Instruction::Call:
            created = SALAM::createCallInst(id, this, debug(), OpCode,
                                            hw->cycle_counts->call_inst,
                                            functional_unit);
            break;
        case llvm::Instruction::Select:
            created = SALAM::createSelectInst(id, this, debug(), OpCode,
                                              hw->cycle_counts->select_inst,
                                              functional_unit);
            break;
        default: {
            warn("Tried to create instance of undefined instruction type!");
            created = SALAM::createBadInst(id, this, dbg, OpCode, 0, 0);
            break;
        }
    }

    assert(created);
    created->setHWInterface(hw);
    return created;
}
