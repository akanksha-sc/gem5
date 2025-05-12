#ifndef __HWACC_ACC_COMPUTE_UNIT_HH__
#define __HWACC_ACC_COMPUTE_UNIT_HH__
#include "hwacc/HWModeling/src/hw_interface.hh"
#include "hwacc/LLVMRead/src/debug_flags.hh"
#include "hwacc/LLVMRead/src/mem_request.hh"
#include "hwacc/comm_interface.hh"
#include "params/AccComputeUnit.hh"
#include "sim/sim_object.hh"

class AccComputeUnit : public SimObject
{
  private:

  protected:
    CommInterface *comm;
    HWInterface* hw;

    class TickEvent : public Event
    {
      private:
        AccComputeUnit *acc_comp_unit;

      public:
        TickEvent(AccComputeUnit *_acc_comp_unit) : Event(CPU_Tick_Pri),
        acc_comp_unit(_acc_comp_unit) {}
        void process() { acc_comp_unit->tick(); }
        virtual const char *description() const
        { return "AccComputeUnit tick"; }
    };


    TickEvent tickEvent;
    int clock_period;

  public:
    virtual void tick() {}
    AccComputeUnit(const AccComputeUnitParams &p);
    virtual void initialize() {}
    virtual void readCommit(MemoryRequest * req) {}
    virtual void writeCommit(MemoryRequest * req) {}
    CommInterface * getCommInterface() { return comm; }
    HWInterface * getHWInterface() { return hw; }
};

#endif //__HWACC_COMPUTE_UNIT_HH__
