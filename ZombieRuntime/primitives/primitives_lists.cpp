#include "pch.h"
#include "../pch.h"
#include "primitives_lists.h"
#include "../Environment.h"
#include "../cons_cell.h"
#include "../Base.h"
#include "../../GarbageCollector/gc_ptr.h"

namespace primitives {

// Helper function for apply primitive - appends two lists
// Used when apply needs to combine individual args with a final list arg
Object* append_lists_for_apply(Object* list1, Object* list2) {
    // If first list is nil, return second list
    if (list1 == NULL || list1 == nil) {
        return list2;
    }

    // Build a new list by recursively consing elements
    Object* first_elem = first(list1);
    Object* rest_of_list1 = rest(list1);

    if (rest_of_list1 == NULL || rest_of_list1 == nil) {
        // Last element of list1 - cons it with list2
        return gc_new<ConsCell>(first_elem, list2);
    } else {
        // Recursively append - protect intermediate result
        Object* rest_result = append_lists_for_apply(rest_of_list1, list2);
        gc_root<Object> rest_guard(rest_result);
        return gc_new<ConsCell>(first_elem, rest_result);
    }
}

} // namespace primitives
