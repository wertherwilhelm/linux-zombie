#pragma once

#include <variant>
#include <type_traits>
#include <stdexcept>
#include <ostream>

/**
 * @brief A class for handling Lisp-like numeric expressions with automatic type casting.
 *
 * The `lisp_number` class provides arithmetic operators (+, -, *, /) and type casts
 * (toInt, toLong, toUnsigned, toDouble) for numeric types, which are automatically
 * converted to the appropriate type based on the operands. The class supports int, long,
 * unsigned, and double types. The arithmetic operators return a new `lisp_number`
 * Object with the result of the operation, while the type casts return the value of
 * the `lisp_number` Object casted to the corresponding type.
 *
 * Example usage:
 * @code
 * lisp_number a(2);
 * lisp_number b(3.5);
 *
 * // Automatically casted to double
 * lisp_number c = a + b;
 *
 * // Automatically casted to int
 * int d = c.toInt();
 *
 * // Throws std::logic_error for division by zero
 * lisp_number e = a / lisp_number(0);
 * @endcode
 */
class lisp_number {
public:
    // Constructors
    lisp_number() = default;
    lisp_number(int value) : _value(value) {}
    lisp_number(long value) : _value(value) {}
    lisp_number(unsigned value) : _value(value) {}
    lisp_number(double value) : _value(value) {}
    lisp_number(const lisp_number& other) = default;
    lisp_number(lisp_number&& other) = default;

    lisp_number& operator=(const lisp_number& other) = default;
    lisp_number& operator=(lisp_number&& other) = default;

    template<typename T1, typename T2,
        typename = std::enable_if_t<std::is_arithmetic_v<T1>&& std::is_arithmetic_v<T2>>>
    lisp_number(T1 v1, T2 v2) : _value(compute_value(v1, v2)) {}

    // Operators
    lisp_number operator+(const lisp_number& other) const {
        return lisp_number(compute_value(toDouble(_value), toDouble(other._value)));
    }

    lisp_number operator-(const lisp_number& other) const {
        return lisp_number(compute_value(toDouble(_value), -toDouble(other._value)));
    }

    lisp_number operator*(const lisp_number& other) const {
        return lisp_number(compute_value(toDouble(_value), toDouble(other._value)));
    }

    lisp_number operator/(const lisp_number& other) const {
        if (toDouble(other._value) == 0.0) {
            throw std::logic_error("Division by zero");
        }
        return lisp_number(toDouble(_value) / toDouble(other._value));
    }

    lisp_number& operator+=(const lisp_number& other) {
        _value = compute_value(toDouble(_value), toDouble(other._value));
        return *this;
    }

    lisp_number& operator-=(const lisp_number& other) {
        _value = compute_value(toDouble(_value), -toDouble(other._value));
        return *this;
    }

    lisp_number& operator*=(const lisp_number& other) {
        _value = compute_value(toDouble(_value), toDouble(other._value));
        return *this;
    }

    lisp_number& operator/=(const lisp_number& other) {
        if (toDouble(other._value) == 0) {
            throw std::logic_error("Division by zero");
        }
        _value = compute_value(toDouble(_value), 1.0 / toDouble(other._value));
        return *this;
    }

    // Casts
    int toInt() const {
        return static_cast<int>(toDouble(_value));
    }

    long toLong() const {
        return static_cast<long>(toDouble(_value));
    }

    unsigned toUnsigned() const {
        return static_cast<unsigned>(toDouble(_value));
    }

    double toDouble() const {
        return toDouble(_value);
    }

    // Stream operator
    friend std::ostream& operator<<(std::ostream& os, const lisp_number& number) {
        os << number.toDouble();
        return os;
    }

private:
    std::variant<int, long, unsigned, double> _value;

    template<typename T1, typename T2>
    static std::common_type_t<T1, T2> compute_value(T1 v1, T2 v2) {
        if constexpr (std::is_same_v<int, T1> && std::is_same_v<int, T2>) {
            return static_cast<int>(v1 + v2);
        } else if constexpr (std::is_same_v<long, T1> && std::is_same_v<long, T2>) {
            return static_cast<long>(v1 + v2);
        } else if constexpr (std::is_same_v<unsigned, T1> && std::is_same_v<unsigned, T2>) {
            return static_cast<unsigned>(v1 + v2);
        } else {
            return static_cast<double>(v1 + v2);
        }
    }

    static double toDouble(std::variant<int, long, unsigned, double> value) {
        return std::visit([](auto x) -> double { return static_cast<double>(x); }, value);
    }
};
