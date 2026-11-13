#include "pch.h"
#include "i_debugger.h"

namespace zombie { namespace runtime {

// Global default debugger (starts as no-op)
std::shared_ptr<IDebugger> g_default_debugger = std::make_shared<NullDebugger>();

} } // namespace zombie::runtime
