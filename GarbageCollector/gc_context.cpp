#include "pch.h"
#include "gc_context.h"

// Thread-local storage for current GC instance
// Each thread maintains its own independent GC context
thread_local IGarbageCollector* gc_context::m_current_gc = nullptr;
