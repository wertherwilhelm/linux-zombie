#pragma once
#include "gc_context.h"
#include "gc_tracer.h"
#include <functional>

namespace zombie::compilation {
// Registers pointer slots, rather than embedding GC addresses in generated code.
// The collector must outlive its compiled artifacts, as with gc_ptr handles.
class ScopedRootTracer {
    IGarbageCollector* gc_;
    int id_;
public:
    explicit ScopedRootTracer(std::function<void(Tracer&)> trace)
        : gc_(gc_context::current()), id_(gc_ ? gc_->register_root_tracer(std::move(trace)) : -1) {}
    ~ScopedRootTracer() { if (gc_ && id_ >= 0) gc_->unregister_root_tracer(id_); }
    ScopedRootTracer(const ScopedRootTracer&) = delete;
    ScopedRootTracer& operator=(const ScopedRootTracer&) = delete;
};
} // namespace zombie::compilation
