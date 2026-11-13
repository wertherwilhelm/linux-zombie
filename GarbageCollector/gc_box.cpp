#include "pch.h"
#include "gc_box.h"
#include "gc_object.h"
#include "gc_allocator.h"

namespace gc_box {

// Default allocator for default allocate/deallocate
// Note: declared as 'extern' in gc_box.h
MallocAllocator default_allocator;

// Legacy deallocate function (uses MallocAllocator)
// Kept for backward compatibility with code that doesn't pass allocator
void deallocate(void* obj) {
    if (!obj) return;
    deallocate(default_allocator, obj);
}

} // namespace gc_box
