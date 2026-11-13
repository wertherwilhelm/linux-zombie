#pragma once

/**
 * @file all.h
 * @brief Convenience header including all standard memory policies
 *
 * Include this to get access to all built-in policies:
 * - GCPolicy - Garbage collected memory
 * - SharedPtrPolicy - Reference counted (std::shared_ptr)
 * - RawPtrPolicy - Manual management (raw pointers)
 *
 * Usage:
 *   #include <MemoryManagement/policies/all.h>
 *
 *   using namespace memory::policies;
 *
 *   template<typename Policy = GCPolicy>
 *   class MyDataStructure { ... };
 */

#include "GCPolicy.h"
#include "SharedPtrPolicy.h"
#include "RawPtrPolicy.h"
#include "../concepts.h"

namespace memory {
namespace policies {

/**
 * @brief Policy selection helper
 *
 * Allows runtime selection of policy (for testing/comparison).
 *
 * Example:
 *   enum class MemoryStrategy { GC, SharedPtr, Raw };
 *
 *   template<MemoryStrategy S>
 *   using SelectPolicy = typename PolicySelector<S>::type;
 *
 *   using MyNode = Node<SelectPolicy<MemoryStrategy::GC>>;
 */
template<int N>
struct PolicySelector;

template<>
struct PolicySelector<0> { using type = GCPolicy; };

template<>
struct PolicySelector<1> { using type = SharedPtrPolicy; };

template<>
struct PolicySelector<2> { using type = RawPtrPolicy; };

/**
 * @brief Default policy (GC if available, otherwise SharedPtr)
 *
 * Use this when you want GC by default but fallback to shared_ptr
 * if GC is not available.
 */
#ifdef MEMORY_MANAGEMENT_GC_AVAILABLE
using DefaultPolicy = GCPolicy;
#else
using DefaultPolicy = SharedPtrPolicy;
#endif

/**
 * @brief Performance-oriented default (Raw if performance critical, GC otherwise)
 */
#ifdef MEMORY_MANAGEMENT_PERFORMANCE_MODE
using PerformancePolicy = RawPtrPolicy;
#else
using PerformancePolicy = GCPolicy;
#endif

/**
 * @brief Safety-oriented default (SharedPtr for cycle safety)
 */
using SafePolicy = SharedPtrPolicy;

/**
 * @brief Compare policy capabilities at compile time
 *
 * Example:
 *   static_assert(PolicyHasGC<GCPolicy>);
 *   static_assert(!PolicyHasGC<RawPtrPolicy>);
 */
template<typename Policy>
concept PolicyHasGC = Policy::supports_gc;

template<typename Policy>
concept PolicyHasTracing = Policy::supports_tracing;

template<typename Policy>
concept PolicyHasBarriers = Policy::supports_barriers;

template<typename Policy>
concept PolicyHasRooting = Policy::supports_rooting;

/**
 * @brief Get human-readable policy name
 */
template<typename Policy>
constexpr const char* policy_name() {
    if constexpr (std::is_same_v<Policy, GCPolicy>) {
        return "GCPolicy";
    } else if constexpr (std::is_same_v<Policy, SharedPtrPolicy>) {
        return "SharedPtrPolicy";
    } else if constexpr (std::is_same_v<Policy, RawPtrPolicy>) {
        return "RawPtrPolicy";
    } else {
        return "CustomPolicy";
    }
}

/**
 * @brief Print policy capabilities (for debugging)
 */
template<typename Policy>
void print_policy_info() {
    std::cout << "Policy: " << policy_name<Policy>() << "\n";
    std::cout << "  Supports GC: " << (Policy::supports_gc ? "Yes" : "No") << "\n";
    std::cout << "  Supports Tracing: " << (Policy::supports_tracing ? "Yes" : "No") << "\n";
    std::cout << "  Supports Barriers: " << (Policy::supports_barriers ? "Yes" : "No") << "\n";
    std::cout << "  Supports Rooting: " << (Policy::supports_rooting ? "Yes" : "No") << "\n";
}

} // namespace policies
} // namespace memory
