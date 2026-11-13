# Garbage Collection Architecture and Programmer Guide

This guide describes the garbage collection (GC) subsystem in this repo and
how to use it safely from C++ code. It covers architecture, APIs, and concrete
usage examples.

---

## Scope and goals

- Explain how the GC is built and how components fit together.
- Provide safe usage patterns for mark-sweep and moving collectors.
- Show example code for common tasks.

This guide focuses on the C++ GC library in `GarbageCollector/` and how
`ZombieLib/` integrates with it.

---

## Architecture overview

High-level flow:

```
User code
  -> gc_manager / gc_manager_t::allocate<T>()
     -> gc_box::allocate<T>()  (gc_header + object layout)
     -> Strategy::on_allocate()
  -> GC collect()
     -> Strategy::mark() (Tracer -> trace_references())
     -> Strategy::sweep() or copy/promotion
```

Key components:

- `gc_object` (GarbageCollector/gc_object.h)
  - Base class for all GC-managed objects.
  - Requires `trace_references(Tracer&)`.
- `Tracer` (GarbageCollector/gc_tracer.h)
  - Visitor used by GC to traverse object graphs.
  - Use `visit_field()` for moving collectors to update pointer locations.
- `gc_box` (GarbageCollector/gc_box.h)
  - Allocation wrapper that adds `gc_header` and alignment padding.
  - Stores type-erased trace and destructor functions per allocation.
- `IAllocator` (GarbageCollector/gc_allocator.h)
  - Pluggable allocation backend (malloc, pool, arena, etc.).
- `gc_manager` / `gc_manager_t` (GarbageCollector/gc_manager*.h)
  - Public GC interface and manager implementation.
  - Strategy-based or template-based selection.
- GC strategies:
  - `MarkSweepStrategy` (GarbageCollector/mark_sweep_strategy.*)
  - `GenerationalGCStrategy` (GarbageCollector/generational_gc_strategy.*)
- `gc_context` (GarbageCollector/gc_context.h)
  - Thread-local GC instance management.

---

## Memory layout (gc_box)

`gc_box::allocate()` creates a single block containing a header and object:

```
[gc_header][padding][size_t offset][Object]
```

The offset (stored immediately before the object) allows `gc_box::get_header()`
to find the header even with alignment padding.

Important `gc_header` fields:

- `tracer`: type-erased trace function for the object type.
- `destructor`: type-erased destructor function.
- `size`: object size in bytes.
- `alignment`: alignment requirement for the object type.
- `padding`: bytes between header and object.
- `marked`: mark bit (mark-sweep).
- `forwarding_addr`: used by copying collectors.

---

## Tracing contract and pointer updates

All GC-managed types must implement:

```cpp
void trace_references(Tracer& tracer) override;
```

### Use `visit_field` (moving GC safe)

`visit_field()` passes the pointer LOCATION and allows moving collectors to
update it after copying.

```cpp
struct node : gc_object {
    node* child = nullptr;

    void trace_references(Tracer& tracer) override {
        tracer.visit_field(child);
    }
};
```

### Handling casts (update the source)

If you must cast, update the original field after visiting:

```cpp
if (parent_env) {
    object* env_obj = static_cast<object*>(parent_env);
    tracer.visit_field(env_obj);
    parent_env = static_cast<enviroment*>(env_obj);  // update after move
}
```

### Containers

Iterate by reference to update elements in-place:

```cpp
for (object*& obj : frame.constants) {
    tracer.visit_field(obj);
}
```

---

## Root management

Roots are objects that must stay alive even if they are not reachable from
other objects. There are two root APIs:

### Value-based roots (mark-sweep friendly)

```cpp
add_root(gc_object* obj);
remove_root(gc_object* obj);
```

These store pointer values in the root set. They are safe for mark-sweep
collectors or objects that never move. For moving collectors, they cannot
update the caller's pointer after relocation.

### Slot-based roots (moving GC safe)

```cpp
register_persistent_root(gc_object** slot);
unregister_persistent_root(gc_object** slot);
```

These store pointer locations and update them after moves.

### RAII helpers

- `gc_root<T>`: Temporary local root (copyable).
- `gc_ptr<T>`: Long-lived ownership root (copyable, comparable).
- `gc_root_guard`: Non-copyable legacy guard (slot-based).
- `gc_persistent_root`: Heap-stored root for embedding APIs.

`gc_root` and `gc_ptr` use slot-based registration internally and always read
the current location after a move.

---

## Allocation and collection

### Default GC (mark-sweep)

```cpp
using default_gc = gc_manager_t<MarkSweepStrategy, MallocAllocator>;

default_gc gc;
auto* node = gc.allocate<node>();

gc.add_root(node);
gc.collect();
gc.remove_root(node);
```

### Rooted locals with gc_root

```cpp
default_gc gc;
gc_context::set_current(&gc);

auto* raw = gc.allocate<node>();
gc_root<node> root(raw);  // slot-based root

gc.collect();  // root pointer is auto-updated if objects move
gc_context::set_current(nullptr);
```

### Long-lived ownership with gc_ptr

```cpp
default_gc gc;
gc_context::set_current(&gc);

gc_ptr<node> handle(gc.allocate<node>());
handle->child = gc.allocate<node>();

gc.collect();
gc_context::set_current(nullptr);
```

### Persistent roots for embedding APIs

```cpp
default_gc gc;
gc_context::set_current(&gc);

node* obj = gc.allocate<node>();
gc_persistent_root root(obj);  // captures owning GC and registers slot

gc.collect();
gc_context::set_current(nullptr);
```

---

## Strategy details

### Mark-sweep (MarkSweepStrategy)

- Stop-the-world, iterative marking (worklist).
- Sweep deletes unmarked objects.
- Supports reentrant allocation during marking by queueing new allocations.

Use this when you want simplicity and non-moving collection.

### Generational (GenerationalGCStrategy)

Young generation uses a semispace copying collector (Cheney).
Old generation uses mark-sweep.

Key rules:

- Use `visit_field()` everywhere in `trace_references()`.
- Use slot-based roots (`gc_root`, `gc_ptr`, or persistent roots).
- Record old->young pointers with the write barrier.

Example:

```cpp
using generational_gc = gc_manager_t<GenerationalGCStrategy, MallocAllocator>;

generational_gc gc;
gc_context::set_current(&gc);

// Promote an object to old gen (example uses allocate_young for tests)
node** old_ref = new node*(gc.strategy().allocate_young<node>());
gc.register_persistent_root(reinterpret_cast<gc_object**>(old_ref));
for (int i = 0; i < 3; ++i) {
    gc.strategy().minor_collect();
}

// Allocate young object and link from old object
node** young_ref = new node*(gc.strategy().allocate_young<node>());
gc.register_persistent_root(reinterpret_cast<gc_object**>(young_ref));
(*old_ref)->child = *young_ref;
gc.strategy().record_write_barrier(*old_ref, *young_ref);

gc.strategy().minor_collect();  // young survives due to remembered set

gc.unregister_persistent_root(reinterpret_cast<gc_object**>(old_ref));
gc.unregister_persistent_root(reinterpret_cast<gc_object**>(young_ref));
delete old_ref;
delete young_ref;
gc_context::set_current(nullptr);
```

Note: `gc_manager_t<GenerationalGCStrategy>::allocate()` currently uses
`gc_box::allocate()` (old generation). Use `allocate_young()` when you need
explicit nursery behavior in tests.

---

## Thread-local GC context

Use `gc_context` to set or swap the active GC for the current thread.

```cpp
default_gc gc1;
default_gc gc2;

gc_context::set_current(&gc1);
{
    gc_scope swap(&gc2);  // temporarily switches to gc2
    auto* obj = gc_context::current()->allocate<node>();
}
```

Construct `gc_root`/`gc_ptr` while the correct GC is active, or pass the GC
explicitly via their constructor.

---

## Low-level APIs (advanced)

### gc_box

`gc_box` can be used directly for allocation and tracing when you need manual
control:

```cpp
auto* obj = gc_box::allocate<MyType>(args...);
gc_box::trace(tracer, obj);
gc_box::deallocate(obj);
```

### Trace<T> specialization

If you need custom tracing for non-`gc_object` types, specialize `Trace<T>`
in `gc_traits.h` to control how child references are enumerated.

---

## Common pitfalls

- Using `tracer.visit()` for moving GC. Use `visit_field()` instead.
- Rooting with `add_root()` and then dereferencing after a moving GC.
  Prefer `gc_root`, `gc_ptr`, or `register_persistent_root`.
- Forgetting `record_write_barrier()` for old->young assignments in
  generational GC.
- Holding raw pointers across GC without slot-based rooting.
- Creating `gc_root`/`gc_ptr` with the wrong `gc_context`.

---

## Suggested reading

- `GarbageCollector/GC_MOVING_COLLECTOR_FIXES_COMPLETE.md`
- `GarbageCollector/TRACER_MIGRATION_GUIDE.md`
- `GarbageCollector/ROOT_API_FIXES_COMPLETE.md`
- `ZombieLib/GC_IMPLEMENTATION_SUMMARY.md`
*** End Patch"}}
