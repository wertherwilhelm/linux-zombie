#pragma once

#include <string>
#include <string_view>

namespace zombie { namespace runtime {

/**
 * @brief Abstract interface for evaluation engines.
 *
 * Allows different Scheme engines or REPLs to be plugged into the
 * ReplSession framework.
 *
 * Implementations should:
 * - Maintain their own environment/state
 * - Support reset for starting fresh
 * - Optionally capture stdout for embedding scenarios
 */
class IEvalEngine {
public:
    virtual ~IEvalEngine() = default;

    /**
     * @brief Reset the engine to initial state.
     *
     * Clears environment, reinstalls primitives, etc.
     */
    virtual void reset() = 0;

    /**
     * @brief Load prelude/standard library (optional).
     *
     * Called after construction and after reset.
     */
    virtual void warmup_prelude() {}

    /**
     * @brief Enable/disable stdout capture mode.
     * @param enable True to capture stdout, false to disable.
     */
    virtual void set_capture_stdout(bool enable) { (void)enable; }

    /**
     * @brief Clear captured stdout buffer.
     */
    virtual void clear_captured_stdout() {}

    /**
     * @brief Get and clear captured stdout.
     * @return Captured stdout text.
     */
    virtual std::string take_captured_stdout() { return {}; }

    /**
     * @brief Optional hook called before evaluation.
     */
    virtual void begin_eval() {}

    /**
     * @brief Optional hook called after evaluation.
     */
    virtual void end_eval() {}

    /**
     * @brief Evaluate source code (one or more top-level forms).
     * @param src Source code to evaluate.
     * @return Opaque pointer to result (typically Object*), or nullptr on error.
     */
    virtual void* eval_string(std::string_view src) = 0;

    /**
     * @brief Convert result to a printable string.
     * @param value Opaque pointer returned by eval_string.
     * @return Printable representation of the value.
     */
    virtual std::string print_value(void* value) = 0;
};

} } // namespace zombie::runtime
