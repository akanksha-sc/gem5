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

#ifndef __HWACC_IO_ACC_HH__
#define __HWACC_IO_ACC_HH__

#include <cstdio>
#include <cstdlib>
#include <queue>

#include "base/trace.hh"
#include "dev/arm/base_gic.hh"
#include "dev/io_device.hh"
#include "hwacc/LLVMRead/debug_flags.hh"
#include "mem/packet.hh"
#include "mem/packet_access.hh"
#include "params/IOAcc.hh"
#include "sim/system.hh"

class IOAcc : public BasicPioDevice
{
  private:
    Addr io_addr;
    Addr io_size;
    std::string devname;
    BaseGic *gic;
    uint32_t int_num;

    class MemSidePort : public RequestPort
    {
      friend class IOAcc;

      private:
        IOAcc *owner;
        std::queue<PacketPtr> outstandingPkts;

      public:
        MemSidePort(const std::string& name, IOAcc *owner) :
          RequestPort(name, owner), owner(owner)
        { }

      protected:
        virtual bool recvTimingResp(PacketPtr pkt);
        virtual void recvReqRetry();
        virtual void recvRangeChange() { };
        virtual Tick recvAtomic(PacketPtr pkt) {return 0;}
        virtual void recvFunctional(PacketPtr pkt) { };
        void setStalled(PacketPtr pkt)
        {
          outstandingPkts.push(pkt);
        }
        bool isStalled() { return !outstandingPkts.empty(); }
        void sendPacket(PacketPtr pkt);
    };

    class TickEvent : public Event
    {
      private:
        IOAcc *acc;

      public:
        TickEvent(IOAcc *_acc) : Event(CPU_Tick_Pri), acc(_acc) {}
        void process() { acc->tick(); }
        virtual const char *description() const { return "IOAcc tick"; }
    };

    MemSidePort memPort;
    MemSidePort* dataPort;
    IOAcc *acc;
    MasterID masterId;
    TickEvent tickEvent;
    unsigned int cacheLineSize;
    unsigned int cacheSize;

    void tick();

    bool needToRead;
    bool needToWrite;
    Addr currentReadAddr;
    Addr currentWriteAddr;
    Addr beginAddr;
    Tick writeLeft;
    Tick writeDone;
    Tick readLeft;
    Tick readDone;
    Tick totalLength;

    uint8_t *curData;
    bool *readsDone;
    bool running;
    bool computationNeeded;

    void tryRead();
    void tryWrite();

    Addr dataAddr;

    uint8_t *mmreg;
    uint32_t mmrval;

    bool processingDone;
    int processDelay;
    int clock_period;

  public:
    typedef IOAccParams Params;
    const Params *
    params() const
    {
      return dynamic_cast<const Params *>(_params);
    }

    IOAcc(Params *p);

    virtual Tick read(PacketPtr pkt);

    virtual Tick write(PacketPtr pkt);

    Port& getPort(const std::string& if_name,
                                  PortID idk = InvalidPortID) override;

    void recvPacket(PacketPtr pkt);

    int prepRead(Addr src, size_t length);
    int prepWrite(Addr dst, uint8_t* value, size_t length);
    int getCacheSize() { return cacheSize; }
    void processData();

    uint8_t* getCurData() { return curData; }

    bool isRunning() { return running; }
    bool isCompNeeded() { return computationNeeded; }

    uint64_t getMMRData(unsigned index) {
            return *(uint64_t *)(mmreg + DEV_MEM_LOC + index * 8);
    }
    int getProcessDelay() { return processDelay; }

  protected:
    static const int DEV_CONFIG = 0x00;
    static const int DEV_MEM_LOC = 0x04;
};

#endif //__HWACC_IO_ACC_HH__

/*
* MM Register Layout
* | Location of Data 32bits | Compute Finished 1bit | Unused 30bits |
* | Start Operation 1bit |
*/
