from m5.objects.ClockedObject import ClockedObject
from m5.params import *
from m5.proxy import *


class SALAMTickEngine(ClockedObject):
    type = "SALAMTickEngine"
    cxx_header = "salam/tick_engine.hh"

    autostart = Param.Bool(False, "Start ticking at startup() if true")
