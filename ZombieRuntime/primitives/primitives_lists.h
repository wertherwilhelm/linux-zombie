#pragma once

#include "../primitive_base.h"

namespace primitives {

// Helper function for apply primitive - appends two lists
// Used when apply needs to combine individual args with a final list arg
Object* append_lists_for_apply(Object* list1, Object* list2);

} // namespace primitives
