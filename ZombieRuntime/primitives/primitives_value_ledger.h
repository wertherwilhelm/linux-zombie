#pragma once

/**
 * @file primitives_value_ledger.h
 * @brief Scheme primitives for Value Ledger and Multiverse system
 *
 * This exposes the Value Ledger infrastructure to Scheme code:
 * - Query value provenance (backend, origin, traits)
 * - Apply skins (transform values between realities)
 * - Capture/restore snapshots (time-travel debugging)
 * - Create mirrors (cross-language calls)
 *
 * Example usage in Scheme:
 *   (define x 42)
 *   (value-info x)  ; => Shows backend, origin, traits
 *   (use-skin x 'native)  ; Transform to native backend
 *   (define snap (capture-snapshot "before-optimization"))
 *   (restore-snapshot snap)  ; Time travel!
 */

#include "../primitive_base.h"

namespace primitives {

/**
 * @brief Get detailed information about a value's ledger entry
 *
 * Scheme: (value-info value)
 * Returns: Association list with backend, origin, traits
 *
 * Example:
 *   (value-info 42)
 *   ; => ((backend . scheme) (origin . "unknown") (traits . ""))
 */
Object* prim_value_info(Object* args, Environment* env);

/**
 * @brief Get stable ValueId for an Object (GC-safe identifier)
 *
 * Scheme: (value-id value)
 * Returns: Integer (uint64_t) uniquely identifying this Object
 *
 * Example:
 *   (value-id x)  ; => 12345
 *   (value-id x)  ; => 12345 (same Object, same ID!)
 *
 * Notes:
 * - IDs are stable across GC (unlike pointers)
 * - ID 0 means "not tracked by ledger"
 * - Calling this on an Object assigns it an ID if it doesn't have one
 */
Object* prim_value_id(Object* args, Environment* env);

/**
 * @brief Apply a skin to a value (transform behavior)
 *
 * Scheme: (use-skin value skin-name)
 * Returns: The value (modified in-place)
 *
 * Example:
 *   (use-skin x 'native)  ; Transform to native backend
 *   (use-skin x 'debug)   ; Add debug traits
 */
Object* prim_use_skin(Object* args, Environment* env);

/**
 * @brief Get the backend tag of a value
 *
 * Scheme: (value-backend value)
 * Returns: Symbol 'scheme, 'native, 'bytecode, or 'dream
 *
 * Example:
 *   (value-backend 42)  ; => 'scheme
 */
Object* prim_value_backend(Object* args, Environment* env);

/**
 * @brief Get the origin string of a value
 *
 * Scheme: (value-origin value)
 * Returns: String describing where value came from
 *
 * Example:
 *   (value-origin x)  ; => "repl:line-5"
 */
Object* prim_value_origin(Object* args, Environment* env);

/**
 * @brief Get the traits string of a value
 *
 * Scheme: (value-traits value)
 * Returns: Space-separated traits string
 *
 * Example:
 *   (value-traits x)  ; => "safe debuggable"
 */
Object* prim_value_traits(Object* args, Environment* env);

/**
 * @brief Capture a ledger snapshot (time crystal)
 *
 * Scheme: (capture-snapshot description)
 * Returns: Snapshot ID (integer)
 *
 * Example:
 *   (define snap (capture-snapshot "before-optimization"))
 *   ; ... do stuff ...
 *   (restore-snapshot snap)  ; Time travel!
 */
Object* prim_capture_snapshot(Object* args, Environment* env);

/**
 * @brief Restore a ledger snapshot (time travel)
 *
 * Scheme: (restore-snapshot snapshot-id)
 * Returns: #t on success
 *
 * Example:
 *   (restore-snapshot snap)
 */
Object* prim_restore_snapshot(Object* args, Environment* env);

/**
 * @brief Check if a value is a mirror (cross-language callable)
 *
 * Scheme: (mirror? value)
 * Returns: #t if value is a mirror, #f otherwise
 *
 * Example:
 *   (mirror? native-sqrt)  ; => #t
 */
Object* prim_is_mirror(Object* args, Environment* env);

/**
 * @brief List all registered skins
 *
 * Scheme: (list-skins)
 * Returns: List of skin name symbols
 *
 * Example:
 *   (list-skins)  ; => (scheme native bytecode debug)
 */
Object* prim_list_skins(Object* args, Environment* env);

/**
 * @brief Count values by backend
 *
 * Scheme: (backend-stats)
 * Returns: Association list of (backend . count)
 *
 * Example:
 *   (backend-stats)
 *   ; => ((scheme . 42) (native . 7) (dream . 3))
 */
Object* prim_backend_stats(Object* args, Environment* env);

/**
 * @brief Find all values with a specific trait
 *
 * Scheme: (find-by-trait trait-string)
 * Returns: List of values with that trait
 *
 * Example:
 *   (find-by-trait "optimized")
 *   ; => (list of optimized values)
 */
Object* prim_find_by_trait(Object* args, Environment* env);

/**
 * @brief Register all value ledger primitives with environment
 */
void register_value_ledger_primitives(Environment* env);

} // namespace primitives
