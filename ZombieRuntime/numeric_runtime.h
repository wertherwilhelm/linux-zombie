#pragma once

#include "Object.h"
#include "atom.h"  // For Type enum and atom<T> template
#include <cstdint>

// Forward declarations - types are in zombie::runtime namespace
namespace zombie { namespace runtime {
    class a_rational;
    class a_complex;
} }

// Use types from zombie::runtime namespace
using zombie::runtime::Object;
using zombie::runtime::Type;
using zombie::runtime::a_rational;
using zombie::runtime::a_complex;
template<typename T> using atom = zombie::runtime::atom<T>;

// ============================================================================
// Numeric Runtime - Tower-aware numeric operations for Scheme
// ============================================================================
//
// This module implements R5RS numeric tower with proper exactness preservation.
//
// Numeric Tower (from most general to most specific):
//   complex       (a_complex: real + imag parts)
//   real          (includes all below + inexact doubles)
//   rational      (a_rational: exact num/den)
//   integer       (atom<int>, atom<long long>: exact whole numbers)|
//
// Key Design Principles:
// - Exactness is preserved whenever possible
// - Operations promote to least-general type that can represent result
// - Division always yields exact result when both operands are exact
// - All arithmetic routes through this API for consistency

namespace numeric {

// ============================================================================
// Type Predicates
// ============================================================================

// Check if Object is any kind of number (int, long, double, rational, complex)
bool is_number(const Object* obj);

// Check if number is exact (integer or rational, not double)
bool is_exact(const Object* obj);

// Check if number is inexact (double or complex with inexact components)
bool is_inexact(const Object* obj);

// Check if number represents an integer value (may be stored as rational 5/1)
bool is_integer_value(const Object* obj);

// Check if number is rational (includes integers)
bool is_rational_value(const Object* obj);

// Check if number is real (not complex, or complex with zero imaginary part)
bool is_real_value(const Object* obj);

// Check if number is complex
bool is_complex_value(const Object* obj);

// ============================================================================
// Arithmetic Operations (Tower-aware with promotion)
// ============================================================================

// Addition: promotes to least-general type needed

/**
 * \brief Add two numbers, promoting types as necessary.
 */
Object* add(Object* a, Object* b);

/**
 * \brief Subtract two numbers, promoting types as necessary.
 */
Object* sub(Object* a, Object* b);

// Multiplication
Object* mul(Object* a, Object* b);

// Division: yields exact rational when both operands are exact
Object* div(Object* a, Object* b);

// Integer division (quotient)
Object* quotient(Object* a, Object* b);

// Remainder (modulo with sign of dividend)
Object* remainder(Object* a, Object* b);

// Modulo (always non-negative)
Object* modulo(Object* a, Object* b);

// Negation
Object* negate(Object* n);

// Absolute value
Object* abs_value(Object* n);

// ============================================================================
// Comparison Operations (Real numbers only)
// ============================================================================

// Compare two real numbers: returns -1 (a<b), 0 (a==b), 1 (a>b)
// Throws error if either operand is non-real complex
int compare(Object* a, Object* b);

// Equality check (works for complex too)
bool equal(Object* a, Object* b);

// ============================================================================
// Conversion Operations
// ============================================================================

// Convert to inexact (double or complex with double components)
Object* to_inexact(Object* n);

// Convert to exact (integer or rational if possible, error if irrational)
Object* to_exact(Object* n);

// ============================================================================
// Constructors (used by reader and primitives)
// ============================================================================

// Create integer from int64 (chooses int or long based on size)
Object* make_integer(int64_t value);

// Create rational from numerator/denominator (auto-normalizes)
Object* make_rational(int64_t num, int64_t den);

// Create complex from real and imaginary parts
Object* make_complex(Object* real, Object* imag);

// Create inexact double
Object* make_inexact(double value);

// ============================================================================
// Helper Functions
// ============================================================================
 
// Extract numeric value as double (for display/inexact operations)
double to_double(Object* n);

// Extract integer value (returns 0 if not representable as integer)
int64_t to_int64(Object* n);

// Get real part (for complex, returns real component; for others, returns self)
Object* real_part(Object* n);

// Get imaginary part (for complex, returns imag component; for others, returns 0)
Object* imag_part(Object* n);

// Check if rational denominator is 1 (i.e., it's an integer)
bool is_rational_integer(const a_rational* r);

} // namespace numeric
