#include "zombie_runtime_types.h"
#include "native_primitive.h"
#include "Environment.h"
#include "atom.h"
#include "gc_ptr.h"

namespace zombie { namespace runtime {

void install_natives(Environment* env, const NativeEntry* entries, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        const NativeEntry& e = entries[i];
        // GC safety: root the name string before the second allocation.
        gc_root<a_string> name(gc_new<a_string>(e.name));
        env->define(name.get(),
                    gc_new<NativePrimitive>(e.name, e.minArgs, e.maxArgs, e.fn));
    }
}

} } // namespace zombie::runtime
