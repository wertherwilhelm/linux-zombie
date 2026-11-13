# Value Ledger System

## Overview

The **Value Ledger** is a universal value representation system that brings **full provenance tracking** and **swappable behaviors** to ZombieScheme. It's the foundation for the **Multiverse Workbench** - a system where values can exist in multiple parallel realities simultaneously.

## Core Philosophy

Traditional interpreters treat values as opaque data. The Value Ledger treats them as **tracked entities** with:
- **Provenance**: Where did this value come from?
- **Backend**: Which reality is it currently in? (Scheme, Native, Bytecode, Dream)
- **Traits**: What properties does it have? (safe, optimized, experimental, etc.)
- **Behavior**: How does it behave? (via swappable "skins")

This enables powerful features:
- **Time-travel debugging** via snapshots
- **Performance profiling** by backend
- **Genetic programming** via Dream variants
- **Seamless C++ ↔ Scheme interop** via mirrors

## Architecture

### Core Types (value.h)

#### BackendTag
```cpp
enum class BackendTag {
    Scheme,    // Traditional Scheme interpreter
    Native,    // JIT-compiled native code
    Bytecode,  // Bytecode VM
    Dream      // Genetic/speculative variants
};
```

The `BackendTag` identifies which "reality" a value inhabits. Values can transition between realities (via remask operations in the future Workbench).

#### ValueVTable
```cpp
struct ValueVTable {
    std::string (*to_string)(void* ptr);
    bool (*is_truthy)(void* ptr);
    void* (*invoke)(void* fn_ptr, const std::vector<void*>& args);
    void (*destroy)(void* ptr);
    const char* type_name;
};
```

The **VTable** defines a value's **behavior**. This is the "skin" - you can swap vtables at runtime to change how a value behaves without changing its underlying data.

#### LedgerEntry
```cpp
struct LedgerEntry {
    BackendTag backend;                  // Which reality?
    std::string origin;                  // Where from? (e.g., "repl:line-5", "file:test.scm:42")
    std::string traits;                  // Space-separated tags (e.g., "safe optimized")
    size_t continuation_snapshot_id;     // Link to continuation (for time-travel)
};
```

The **LedgerEntry** is the value's **passport** - it tracks the value's entire journey through the system.

#### Value
```cpp
struct Value {
    void* ptr;                   // Underlying data (Scheme object*, native pointer, etc.)
    const ValueVTable* vt;       // Behavior (swappable!)
    LedgerEntry ledger;          // Provenance

    bool is_callable() const;    // Can it be invoked?
    std::string to_string() const;
    bool is_truthy() const;
    void* invoke(const std::vector<void*>& args) const;
};
```

The **Value** is the universal container. Every value in the multiverse is a Value struct.

### Skin System (ledger.h)

#### Skin
```cpp
struct Skin {
    std::string name;            // e.g., "scheme", "native", "debug"
    ValueVTable vt;              // Behavior implementation
    std::string default_traits;  // Traits added when skin applied
};
```

A **Skin** is a named behavior template. You register skins with the Ledger, then apply them to values:

```cpp
Ledger ledger;
ledger.register_skin(get_scheme_skin());
ledger.register_skin(get_native_skin());

Value v = wrap_int(42, "demo");
ledger.apply_skin(v, "native");  // Transform to native!
```

#### Ledger
The **Ledger** is the global registry and time-travel database:

- **Skin Registry**: `register_skin()`, `find_skin()`, `apply_skin()`
- **Provenance Tracking**: `record_value()`, `lookup_provenance()`
- **Time Travel**: `capture_snapshot()`, `restore_snapshot()`
- **Analytics**: `count_by_backend()`, `find_by_trait()`

Example - Time Travel:
```cpp
Ledger ledger;
ledger.record_value(v1.ptr, v1.ledger);
ledger.record_value(v2.ptr, v2.ledger);

size_t snapshot = ledger.capture_snapshot("before-optimization");

// Modify values...
v1.ledger.backend = BackendTag::Native;
ledger.record_value(v1.ptr, v1.ledger);

// Restore original state
ledger.restore_snapshot(snapshot);
```

### Mirror System (mirror.h, mirror.cpp)

**Mirrors** are the bridges between realities. They enable seamless cross-language calls.

#### Scheme → C++ (SchemeMirror)
```cpp
class SchemeMirror {
    object* proc_;         // Scheme procedure
    enviroment* env_;      // Lexical environment
    void* invoke(const std::vector<void*>& args);
};

// Usage:
object* scheme_fn = parse_and_eval("(lambda (x) (+ x 1))");
Value mirror = make_scheme_mirror(scheme_fn);
void* result = mirror.invoke({wrap_int(21)});  // Calls Scheme from C++!
```

#### C++ → Scheme (NativeMirror)
```cpp
template<typename Fn>
class NativeMirror {
    Fn fn_;
    void* invoke(const std::vector<void*>& args);
};

// Usage:
auto square = [](double x) { return x * x; };
Value mirror = make_native_mirror(square);
// Now Scheme can call: (mirror 5.0) => 25.0
```

#### MirrorRegistry
Named mirrors for easy lookup:
```cpp
MirrorRegistry* registry = g_mirrors;
registry->register_mirror("math.square", make_native_mirror([](double x) { return x*x; }));

// Later from Scheme:
// (define square (lookup-mirror "math.square"))
// (square 5) => 25
```

### Scheme Skin (scheme_skin.h, scheme_skin.cpp)

The **Scheme skin** wraps existing `object*` values in the Value system.

#### Wrapper Functions
```cpp
Value wrap_scheme_object(object* obj, const std::string& origin);
object* unwrap_scheme_object(const Value& v);

// Type-specific helpers:
Value wrap_int(long value, const std::string& origin = "scheme:int");
Value wrap_double(double value, const std::string& origin = "scheme:double");
Value wrap_string(const std::string& str, const std::string& origin = "scheme:string");
Value wrap_bool(bool value, const std::string& origin = "scheme:bool");
Value wrap_nil(const std::string& origin = "scheme:nil");
```

#### Conversion Helpers
```cpp
object* values_to_list(const std::vector<Value>& values);
std::vector<Value> list_to_values(object* list, const std::string& origin);
```

Convert between Value vectors and Scheme lists seamlessly.

## Usage Examples

### Basic Wrapping
```cpp
#include "value.h"
#include "scheme_skin.h"

Value v_int = wrap_int(42, "demo:basic");
std::cout << v_int.to_string() << "\n";      // "42"
std::cout << v_int.origin() << "\n";         // "demo:basic"
std::cout << v_int.backend() << "\n";        // BackendTag::Scheme
std::cout << v_int.is_truthy() << "\n";      // true
```

### Swappable Skins
```cpp
#include "ledger.h"

Ledger ledger;
ledger.register_skin(get_scheme_skin());

Skin debug_skin = get_scheme_skin();
debug_skin.name = "debug";
debug_skin.default_traits = "verbose logging tracing";
ledger.register_skin(debug_skin);

Value v = wrap_int(100, "demo");
ledger.apply_skin(v, "debug");
// v.traits() now includes "verbose logging tracing"
```

### Time-Travel Debugging
```cpp
Ledger ledger;

Value v1 = wrap_int(100, "timeline:alpha");
ledger.record_value(v1.ptr, v1.ledger);

size_t snapshot = ledger.capture_snapshot("before-optimization");

// Modify value
v1.ledger.backend = BackendTag::Native;
v1.ledger.traits = "optimized jit-compiled";
ledger.record_value(v1.ptr, v1.ledger);

// Restore
ledger.restore_snapshot(snapshot);
const LedgerEntry* restored = ledger.lookup_provenance(v1.ptr);
// restored->backend is BackendTag::Scheme again!
```

### Multiverse Branching
```cpp
Value v_scheme = wrap_int(42, "multiverse:base");

// Reality A: Scheme (safe baseline)
Value reality_a = v_scheme;
reality_a.ledger.traits = "safe interpreted baseline";

// Reality B: Native (fast JIT)
Value reality_b = v_scheme;
reality_b.ledger.backend = BackendTag::Native;
reality_b.ledger.traits = "native optimized jit";

// Reality C: Dream (genetic variant)
Value reality_c = v_scheme;
reality_c.ledger.backend = BackendTag::Dream;
reality_c.ledger.origin = "dream:mutant-137";
reality_c.ledger.traits = "experimental genetic-variant";

// Same value, three realities! Compare performance/behavior.
```

## Demo Program

Run `value_demo.cpp` to see all features in action:
- Demo 1: Basic value wrapping with provenance
- Demo 2: Swappable skins (behavior transformation)
- Demo 3: Full provenance tracking with statistics
- Demo 4: Time-travel via snapshots
- Demo 5: Multiverse branching (parallel realities)

## Integration Points

### With Scheme Evaluator
```cpp
// In lisp::new_eval():
object* result = /* ... evaluation ... */;
Value wrapped = wrap_scheme_object(result, "eval:result");
g_ledger->record_value(wrapped.ptr, wrapped.ledger);
return result;
```

### With Continuation Snapshots
```cpp
// When capturing continuation:
size_t cont_id = g_ledger->capture_snapshot("continuation");
continuation* cont = new continuation(vm_state);
cont->set_ledger_snapshot_id(cont_id);

// When invoking continuation:
g_ledger->restore_snapshot(cont->ledger_snapshot_id());
```

### With Multiverse Workbench (Future)
- Visual timeline showing all snapshots
- Drag nodes to fork realities
- Remask operations to swap backends
- Genetic proposals for Dream variants
- Side-by-side comparison of parallel realities

## Performance Notes

- **Value struct**: 32-40 bytes (ptr + vt pointer + ledger entry)
- **Ledger lookups**: O(1) hash table lookup
- **Snapshot capture**: O(n) where n = number of tracked values
- **Snapshot restore**: O(m) where m = snapshot size

For hot paths, you can:
- Use unwrapped `object*` directly (no tracking overhead)
- Only track values you care about (selective recording)
- Batch snapshot captures (not every evaluation)

## Future Directions

1. **Scheme Primitives**:
   ```scheme
   (value-info v)              ; Read ledger information
   (use-skin v 'skin-name)     ; Swap skins
   (mirror? v)                 ; Check if callable mirror
   (capture-snapshot "desc")   ; Time-travel from Scheme
   ```

2. **Native Skin**: JIT-compiled values with LLVM backend

3. **Dream Engine**: Genetic programming system that:
   - Generates code variants automatically
   - Tracks them as Dream backend values
   - Evaluates fitness via execution
   - Evolves optimal implementations

4. **Multiverse Workbench**: Visual IDE showing:
   - Timeline of snapshots (time crystals)
   - Parallel realities side-by-side
   - Remask operations (backend swaps)
   - Performance metrics per reality

## Credits

Inspired by:
- **Value Ledger Sketch** (docs/future_work/Value_Ledger_Sketch.md)
- **Multiverse Workbench Vision** (docs/future_work/Multiverse_Workbench_Vision.md)
- Genetic programming research
- Time-travel debugging systems
- Multi-reality quantum computing metaphors

The Dream backend represents the most ambitious goal: a system where code **evolves itself** through genetic mutation and selection, with the Value Ledger tracking every variant's provenance and performance.
