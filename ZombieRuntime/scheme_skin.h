#pragma once

/**
 * @file scheme_skin.h
 * @brief Scheme skin - wrap Object* in the Value system
 *
 * This skin allows existing Scheme objects to participate in the multiverse!
 * It provides a vtable that implements Value operations using Scheme semantics.
 *
 * Example:
 *   Object* scheme_obj = parse("(lambda (x) (+ x 1))");
 *   Value v = wrap_scheme_object(scheme_obj);
 *
 *   // Now it has full provenance:
 *   std::cout << v.origin() << "\n";     // "scheme:Object"
 *   std::cout << v.backend() << "\n";    // BackendTag::Scheme
 *   std::cout << v.to_string() << "\n";  // "#<Procedure>"
 *
 *   // Can swap to different skin later:
 *   ledger.apply_skin(v, "native");  // Transform to native!
 */

#include "value.h"
#include "ledger.h"

// Forward declarations
namespace zombie { namespace runtime {
class Object;
class Environment;
class Base;
}}
using zombie::runtime::Object;
using zombie::runtime::Environment;
using zombie::runtime::Base;

// ============================================================================
// Scheme VTable - Behavior for Object* wrapped as Value
// ============================================================================

/**
 * @brief VTable for Scheme objects
 *
 * This implements Value operations using Scheme semantics:
 * - to_string: Uses Scheme's print Procedure
 * - is_truthy: Uses Scheme's truth rules (#f = false, everything else = true)
 * - invoke: Calls Scheme apply for procedures
 * - destroy: No-op (GC manages lifecycle)
 */
extern const ValueVTable scheme_vtable;

// ============================================================================
// Scheme Skin Definition
// ============================================================================

/**
 * @brief The "scheme" skin - default behavior for Scheme values
 *
 * This skin can be registered with the Ledger:
 *   ledger.register_skin(get_scheme_skin());
 */
const Skin& get_scheme_skin();

// ============================================================================
// Wrapper Functions
// ============================================================================

/**
 * @brief Wrap a Scheme Object in a Value
 *
 * This is the main entry point for bringing Scheme objects into the
 * multiverse. The wrapped value:
 * - Has backend = Scheme
 * - Uses scheme_vtable for behavior
 * - Can be swapped to other skins later
 *
 * Example:
 *   Object* num = make_int(42);
 *   Value v = wrap_scheme_object(num, "repl:line-5");
 *
 *   std::cout << v.to_string() << "\n";  // "42"
 *   std::cout << v.origin() << "\n";     // "repl:line-5"
 *
 * @param obj Scheme Object to wrap
 * @param origin Where this value came from (for debugging)
 * @return Value wrapping the Object
 */
Value wrap_scheme_object(Object* obj, const std::string& origin = "scheme:Object");

/**
 * @brief Unwrap a Value to get the underlying Scheme Object
 *
 * This extracts the Object* from a Value. Safe to call on any Value,
 * but will throw if the underlying type isn't Object*.
 *
 * @param v Value to unwrap
 * @return Underlying Object* pointer
 * @throws std::runtime_error if value is null or not a Scheme Object
 */
Object* unwrap_scheme_object(const Value& v);

/**
 * @brief Check if a Value wraps a Scheme Object
 */
bool is_scheme_value(const Value& v);

// ============================================================================
// Type-Specific Wrappers (Convenience)
// ============================================================================

/**
 * @brief Wrap a Scheme integer
 */
Value wrap_int(long value, const std::string& origin = "scheme:int");

/**
 * @brief Wrap a Scheme double
 */
Value wrap_double(double value, const std::string& origin = "scheme:double");

/**
 * @brief Wrap a Scheme string
 */
Value wrap_string(const std::string& str, const std::string& origin = "scheme:string");

/**
 * @brief Wrap a Scheme boolean
 */
Value wrap_bool(bool value, const std::string& origin = "scheme:bool");

/**
 * @brief Wrap Scheme nil
 */
Value wrap_nil(const std::string& origin = "scheme:nil");

// ============================================================================
// Conversion Helpers
// ============================================================================

/**
 * @brief Convert Value to Scheme list
 *
 * This takes a vector of Values and creates a Scheme list:
 *   [v1, v2, v3] → (v1 v2 v3)
 *
 * @param values Vector of Values to convert
 * @return Scheme list (Object*)
 */
Object* values_to_list(const std::vector<Value>& values);

/**
 * @brief Convert Scheme list to vector of Values
 *
 * Inverse of values_to_list:
 *   (v1 v2 v3) → [v1, v2, v3]
 *
 * @param list Scheme list
 * @param origin Origin string for wrapped values
 * @return Vector of Values
 */
std::vector<Value> list_to_values(Object* list, const std::string& origin = "scheme:list-elem");
