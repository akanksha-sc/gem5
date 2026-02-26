#ifndef __SALAM_TICK_ENGINE_HH__
#define __SALAM_TICK_ENGINE_HH__

#include "params/SALAMTickEngine.hh"
#include "sim/clocked_object.hh"
#include "sim/core.hh"
#include "sim/eventq.hh"
#include "sim/sim_object.hh"

namespace SALAM
{

class CycleTicked
{
  public:
    virtual ~CycleTicked() = default;

    /**
     * Execute one accelerator cycle of owner behavior.
     *
     * @return true if the engine should continue ticking, false to stop.
     */
    virtual bool tickCycle() = 0;
};

} // namespace SALAM

using namespace gem5;

class SALAMTickEngine : public gem5::ClockedObject
{
  private:
    SALAM::CycleTicked *owner;

    bool active;

    gem5::EventFunctionWrapper tickEvent;

    void tick();

  public:
    PARAMS(SALAMTickEngine);
    SALAMTickEngine(const SALAMTickEngineParams &p);

    void startup() override;
    void
    setOwner(SALAM::CycleTicked *o)
    {
        owner = o;
    }

    // Begin per-cycle ticking on this engine's clock domain.
    void start();

    // Stop ticking. Safe to call even if not active.
    void stop();

    bool
    isActive() const
    {
        return active;
    }
};

#endif // __SALAM_TICK_ENGINE_HH__
