/**
 * @file i_native_code.h
 * @brief Contract for compiled native (machine-code) procedure bodies.
 *
 * Lets a Closure carry native code without ZombieRuntime depending on the
 * native backend: the backend (ZombieNative) implements this interface and
 * hands a Closure a shared_ptr to it. Closure::apply prefers native code when
 * present, exactly as it prefers bytecode for a BytecodeVM.
 */
#pragma once
#include "zombie_runtime_types.h"

class Tracer;

namespace zombie { namespace runtime {

class INativeCode {
public:
    virtual ~INativeCode() = default;

    /**
     * @brief Run the compiled body.
     * @param call_env Environment already extended with the closure's parameter
     *                 bindings (its parent is the closure's captured environment).
     * @return Result Object (non-null on success); throws on error.
     */
    virtual Object* invoke(Environment* call_env) = 0;

    /**
     * @brief Trace managed objects the code holds (e.g. its constant pool) so
     *        they survive collection while the owning Closure is live.
     */
    virtual void trace(Tracer& tracer) = 0;
};

} } // namespace zombie::runtime
