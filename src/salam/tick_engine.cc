#include "salam/tick_engine.hh"

#include "base/logging.hh"
#include "base/trace.hh"
#include "debug/SALAMTickEngine.hh"

using namespace gem5;

SALAMTickEngine::SALAMTickEngine(const SALAMTickEngineParams &p)
    : ClockedObject(p),
      owner(nullptr),
      active(false),
      tickEvent([this] { tick(); }, name())
{}

void
SALAMTickEngine::startup()
{
    ClockedObject::startup();

    if (params().autostart) {
        fatal_if(!owner,
                 "SALAMTickEngine '%s' autostart=true but owner not bound\n",
                 name());
        start();
    }
}

void
SALAMTickEngine::start()
{
    fatal_if(!owner,
             "SALAMTickEngine '%s' has no owner bound. "
             "Did you forget tickEngine->setOwner(this) in the owner's ctor?",
             name());

    if (active) {
        return;
    }

    active = true;

    if (!tickEvent.scheduled()) {
        schedule(tickEvent, nextCycle());
    }

    DPRINTF(SALAMTickEngine, "Start ticking\n");
}

void
SALAMTickEngine::stop()
{
    active = false;

    if (tickEvent.scheduled()) {
        deschedule(tickEvent);
    }

    DPRINTF(SALAMTickEngine, "Stop ticking\n");
}

void
SALAMTickEngine::tick()
{
    if (!active) {
        return;
    }

    fatal_if(!owner, "SALAMTickEngine '%s' ticked with null owner", name());

    const bool keep = owner->tickCycle();
    if (keep) {
        schedule(tickEvent, nextCycle());
    } else {
        active = false;
        DPRINTF(SALAMTickEngine, "Owner requested stop\n");
    }
}
