/**
 * @file scheme_engine.h
 * @brief Unified Scheme interpreter engine - main public API for embedding
 * @author chrislk@gmail.com
 * @date 2024-06
 *
 * Provides a high-level, batteries-included Scheme interpreter that combines
 * environment, evaluator, reader, and I/O ports into a single cohesive Object.
 * This is the recommended entry point for embedding Zombie Scheme in C++
 * applications or building interactive REPLs.
 *
 * **Key Features:**
 * - **Pluggable evaluators** via dependency injection (IEvaluator interface)
 * - **Automatic initialization** (GC, primitives, standard environment)
 * - **Multiple evaluation strategies** (tree-walking, bytecode VM, Continuation-based)
 * - **Integrated parser** (scheme_reader, no legacy tokenizers)
 * - **Configurable I/O** (custom input/output ports)
 * - **REPL-ready** (implements IEvalEngine interface)
 * - **Stdout capture** for embedding scenarios
 *
 * @see IEvalEngine for REPL interface
 * @see IEvaluator for evaluator interface
 * @see embedding_api.h for type-safe embedding wrapper
 */
#pragma once

#include "../ZombieRuntime/zombie_runtime_types.h"
#include "i_evaluator.h"
#include <string>
#include <sstream>
#include <memory>


// Forward declarations
class gc_manager;
class RuntimeContext;

/**
 * @brief Evaluator type selection for scheme_engine
 *
 * Determines which evaluation strategy to use. Each evaluator implements
 * the IEvaluator interface and can be swapped without affecting semantics.
 *
 * **Performance characteristics:**
 * - LISP: Baseline, recursive tree-walking
 * - BYTECODE: ~5-10x faster for compute-intensive code
 * - TOY_VM: Slower but enables first-class continuations (call/cc)
 */
enum class EvaluatorType {
    LISP,       ///< Traditional recursive tree-walking interpreter (default)
    TOY_VM,     ///< Stack-based VM with explicit continuations (call/cc support)
    BYTECODE    ///< Bytecode compiler and virtual machine (high performance)
};

/**
 * @brief Unified Scheme interpreter engine - main public API
 *
 * This class provides a complete, ready-to-use Scheme interpreter that bundles
 * together all the components needed for evaluation: environment, evaluator,
 * parser (reader), and I/O ports.
 *
 * **Design Philosophy:**
 * scheme_engine is designed as a **facade** over the lower-level interpreter
 * components. It handles all the boilerplate setup (GC initialization, Primitive
 * registration, environment creation) and provides a simple, high-level interface
 * for common operations.
 *
 * **Pluggable Evaluators (Dependency Injection):**
 * The engine accepts any Object implementing IEvaluator, allowing you to switch
 * evaluation strategies without changing client code:
 * - **LISP**: Traditional recursive interpreter (production-ready, default)
 * - **TOY_VM**: Stackless VM with explicit continuations (call/cc support)
 * - **BYTECODE**: Ahead-of-time bytecode compiler (production-ready, fast)
 *
 * **IEvalEngine Interface:**
 * Implements the IEvalEngine interface, making it compatible with the REPL
 * infrastructure (ReplSession from repl_core.hpp). This provides a standard
 * protocol for REPLs to interact with the interpreter.
 *
 * **Lifecycle Management:**
 * - Automatically calls init_lisp_constants() to initialize GC
 * - Installs all Primitive functions in global environment
 * - Manages I/O port lifetime (can take ownership or use external ports)
 * - Supports reset() to clear state while preserving evaluator type
 *
 * **Stdout Capture:**
 * For embedding scenarios, you can enable stdout capture to intercept output
 * from (display ...) and other printing operations. Useful for testing or
 * integrating with non-console environments.
 *
 * **Thread Safety:**
 * scheme_engine instances are NOT thread-safe. Each thread should have its
 * own instance if concurrent evaluation is needed.
 *
 * @code
 * // Example 1: Basic usage (default Lisp evaluator)
 * scheme_engine engine;
 * Object* result = engine.eval_string("(+ 1 2)");  // Returns 3
 * std::string output = engine.print_object(result);
 *
 * // Example 2: Use bytecode VM for performance
 * scheme_engine engine_fast(EvaluatorType::BYTECODE);
 * engine_fast.eval_string("(define (fib n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2)))))");
 * Object* result = engine_fast.eval_string("(fib 30)");
 *
 * // Example 3: Dependency injection with custom evaluator
 * IEvaluator* custom = new MyEvaluator();
 * scheme_engine engine_custom(custom, true);  // true = take ownership
 * engine_custom.eval_string("(my-custom-operation)");
 *
 * // Example 4: Stdout capture for embedding
 * scheme_engine engine;
 * engine.set_capture_stdout(true);
 * engine.eval_string("(display \"hello\") (newline)");
 * std::string captured = engine.take_captured_stdout();  // "hello\n"
 *
 * // Example 5: REPL integration (implements IEvalEngine)
 * auto engine = std::make_unique<scheme_engine>();
 * ReplSession repl(std::move(engine));
 * // ... repl.submit_line("(+ 1 2)") ...
 * @endcode
 *
 * @see IEvalEngine for REPL protocol interface
 * @see IEvaluator for evaluator interface
 * @see EvaluatorType for available evaluation strategies
 * @see embedding_api.h for type-safe C++/Scheme interop
 * @see ReplSession (repl_core.hpp) for REPL infrastructure
 */
class scheme_engine : public IEvalEngine {
public:
    /**
     * @brief Construct engine with default Lisp evaluator
     * @param use_isolated_gc If true, creates isolated GC instance (for test isolation)
     * @note Automatically initializes GC and installs all primitives
     * @note Default I/O: stdin for input, stdout for output
     *
     * Example:
     * @code
     * scheme_engine engine;  // Ready to use immediately
     * engine.eval_string("(define x 42)");
     * @endcode
     */
    explicit scheme_engine(bool use_isolated_gc = false);

    /**
     * @brief Construct engine with specified evaluator type
     * @param type Evaluator strategy (LISP, TOY_VM, BYTECODE)
     * @param use_isolated_gc If true, creates isolated GC instance
     * @note Automatically creates evaluator instance based on type
     * @note Automatically initializes GC and installs all primitives
     *
     * Example:
     * @code
     * scheme_engine fast_engine(EvaluatorType::BYTECODE);  // Use bytecode VM
     * fast_engine.eval_string("(define (loop n) (if (> n 0) (loop (- n 1))))");
     * @endcode
     */
    explicit scheme_engine(EvaluatorType type, bool use_isolated_gc = false);

    /**
     * @brief Construct engine with custom evaluator (dependency injection)
     * @param evaluator Custom evaluator implementing IEvaluator interface
     * @param take_ownership If true, engine will delete evaluator on destruction
     * @note Allows complete control over evaluation strategy
     * @note Automatically initializes GC and installs all primitives
     * @note Uses evaluator's existing environment (does not create new one)
     *
     * Example:
     * @code
     * IEvaluator* custom = new MyOptimizingEvaluator();
     * scheme_engine engine(custom, true);  // Engine owns custom evaluator
     * @endcode
     */
    scheme_engine(IEvaluator* evaluator, bool take_ownership = false);

    /**
     * @brief Destructor - cleans up owned resources
     * @note Deletes evaluator if owns_evaluator_ is true
     * @note Deletes I/O ports if owns_input_port_/owns_output_port_ are true
     * @note Shuts down isolated GC if owns_isolated_gc_ is true
     */
    ~scheme_engine();

    // ========================================================================
    // IEvalEngine Interface Implementation (REPL Protocol)
    // ========================================================================

    /**
     * @brief Evaluate Scheme source code string (IEvalEngine interface)
     * @param source Scheme source code (may contain multiple expressions)
     * @return Opaque pointer to result Object (internally Object*)
     * @note Implements IEvalEngine::eval_string for REPL compatibility
     * @note Returns nullptr on parse error (check get_last_error())
     * @note Multiple expressions return result of last expression
     *
     * Example:
     * @code
     * void* result = engine.eval_string("(+ 1 2)");
     * Object* obj = static_cast<Object*>(result);  // Cast to Object*
     * @endcode
     */
    void* eval_string(std::string_view source) override;

    /**
     * @brief Print result value to string (IEvalEngine interface)
     * @param value Opaque pointer from eval_string (internally Object*)
     * @return String representation of the Object
     * @note Implements IEvalEngine::print_value for REPL compatibility
     *
     * Example:
     * @code
     * void* result = engine.eval_string("(list 1 2 3)");
     * std::string output = engine.print_value(result);  // "(1 2 3)"
     * @endcode
     */
    std::string print_value(void* value) override;

    /**
     * @brief Reset interpreter state (IEvalEngine interface)
     * @note Clears global environment and reinstalls all primitives
     * @note Preserves evaluator type (e.g., BYTECODE stays BYTECODE)
     * @note Resets I/O ports to defaults (stdin/stdout)
     *
     * Use case: Clear state between test runs or REPL restarts
     */
    void reset() override;

    /**
     * @brief Load standard library / prelude (IEvalEngine interface)
     * @note Currently a no-op (can be overridden for custom initialization)
     * @note Hook for loading standard library definitions
     */
    void warmup_prelude() override;

    /**
     * @brief Enable/disable stdout capture mode (IEvalEngine interface)
     * @param enable If true, capture output from (display ...) and (write ...)
     * @note When enabled, output goes to internal buffer instead of stdout
     * @note Use take_captured_stdout() to retrieve captured output
     *
     * Example:
     * @code
     * engine.set_capture_stdout(true);
     * engine.eval_string("(display \"hello\")");
     * std::string output = engine.take_captured_stdout();  // "hello"
     * @endcode
     */
    void set_capture_stdout(bool enable) override;

    /**
     * @brief Get captured stdout and clear buffer (IEvalEngine interface)
     * @return String containing all captured output since last call
     * @note Buffer is cleared after retrieval
     * @note Returns empty string if capture is disabled or buffer is empty
     */
    std::string take_captured_stdout() override;

    /**
     * @brief Clear captured stdout buffer without retrieving (IEvalEngine interface)
     * @note Useful to discard output from setup code
     */
    void clear_captured_stdout() override;

    // ========================================================================
    // Direct Evaluation API (Object* instead of void*)
    // ========================================================================

    /**
     * @brief Evaluate a single parsed expression
     * @param expr Parsed Scheme expression (from read_string)
     * @return Result Object
     * @note Lower-level than eval_string - expects pre-parsed expression
     *
     * Example:
     * @code
     * Object* expr = engine.read_string("(+ 1 2)");
     * Object* result = engine.eval_expr(expr);  // Evaluate parsed expression
     * @endcode
     */
    Object* eval_expr(Object* expr);

    /**
     * @brief Parse Scheme source code without evaluating
     * @param source Scheme source code string
     * @return Parsed expression tree or nullptr on parse error
     * @note Check get_last_error() if nullptr returned
     * @note Only parses first expression if source contains multiple
     *
     * Example:
     * @code
     * Object* expr = engine.read_string("(lambda (x) (* x x))");
     * if (expr) {
     *     Object* result = engine.eval_expr(expr);  // Evaluate later
     * }
     * @endcode
     */
    Object* read_string(const std::string& source);

    /**
     * @brief Parse multiple Scheme expressions from string
     * @param source String containing Scheme code
     * @return List of parsed expressions (or nullptr on error)
     * @note Returns a list, not a single expression like read_string()
     *
     * Example:
     * @code
     * Object* program = engine.read_program("(define x 1) (+ x 2)");
     * // Returns: ((define x 1) (+ x 2))
     * @endcode
     */
    Object* read_program(const std::string& source);

    /**
     * @brief Print Object to string representation
     * @param obj Object to print
     * @return String representation (S-expression format)
     * @note Uses Scheme syntax (e.g., "(1 2 3)" for lists)
     *
     * Example:
     * @code
     * Object* list = engine.eval_string("(list 'a 'b 'c)");
     * std::string str = engine.print_object(list);  // "(a b c)"
     * @endcode
     */
    std::string print_object(Object* obj);

    // ========================================================================
    // Accessors
    // ========================================================================

    /**
     * @brief Get global environment
     * @return Pointer to global environment containing all bindings
     * @note Use to access or modify global definitions
     *
     * Example:
     * @code
     * Environment* env = engine.get_environment();
     * Object* val = env->lookup("my-variable");
     * @endcode
     */
    Environment* get_environment() { return global_env_; }

    /**
     * @brief Get evaluator instance
     * @return Pointer to current evaluator (IEvaluator interface)
     * @note Can be cast to concrete type (Lisp*, BytecodeVM*, etc.) if needed
     */
    IEvaluator* get_evaluator() { return evaluator_; }

    /**
     * @brief Get evaluator type
     * @return Type of current evaluator (LISP, TOY_VM, BYTECODE)
     * @note Returns LISP if evaluator was injected via constructor
     */
    EvaluatorType get_evaluator_type() const { return evaluator_type_; }

    /**
     * @brief Get runtime context
     * @return Pointer to RuntimeContext (GC, ports, ledger, etc.)
     * @note Use this to access runtime services in a unified way
     */
    RuntimeContext* get_runtime_context() { return runtime_context_.get(); }

    /**
     * @brief Get reader instance
     * @return Pointer to scheme_reader used for parsing
     * @note Rarely needed directly; use read_string() instead
     */
    scheme_reader* get_reader() { return &reader_; }

    /**
     * @brief Get current input port
     * @return Pointer to input port (default: stdin)
     * @note Used by (read) and (read-char) primitives
     */
    InputPort* get_input_port() { return input_port_; }

    /**
     * @brief Get current output port
     * @return Pointer to output port (default: stdout or capture buffer)
     * @note Used by (display), (write), (newline) primitives
     */
    OutputPort* get_output_port() { return output_port_; }

    /**
     * @brief Get last error message
     * @return Error message from last failed operation, or empty string
     * @note Check this if eval_string returns nullptr
     *
     * Example:
     * @code
     * void* result = engine.eval_string("(invalid syntax");
     * if (!result) {
     *     std::cerr << "Error: " << engine.get_last_error() << std::endl;
     * }
     * @endcode
     */
    std::string get_last_error() const { return last_error_; }

    /**
     * @brief Get last error as a Scheme exception Object
     * @return a_exception Object or nullptr if no error or error was string-only
     * @note Provides structured access to error message, irritants, and kind
     *
     * Example:
     * @code
     * void* result = engine.eval_string("(car 42)");
     * if (!result) {
     *     if (auto* exc = engine.get_last_error_object()) {
     *         std::cerr << "Error: " << exc->message() << std::endl;
     *         Object* irritants = exc->irritants();
     *     }
     * }
     * @endcode
     */
    a_exception* get_last_error_object() const { return last_error_object_; }

    // ========================================================================
    // I/O Port Configuration
    // ========================================================================

    /**
     * @brief Set custom input port
     * @param port New input port (stdin by default)
     * @note Engine does not take ownership; caller must manage lifetime
     * @note Port must outlive the engine or be replaced before destruction
     *
     * Example:
     * @code
     * FileInputPort file_port("input.scm");
     * engine.set_input_port(&file_port);
     * engine.eval_string("(read)");  // Reads from file instead of stdin
     * @endcode
     */
    void set_input_port(InputPort* port);

    /**
     * @brief Set custom output port
     * @param port New output port (stdout by default)
     * @note Engine does not take ownership; caller must manage lifetime
     * @note Port must outlive the engine or be replaced before destruction
     *
     * Example:
     * @code
     * FileOutputPort file_port("output.txt");
     * engine.set_output_port(&file_port);
     * engine.eval_string("(display \"hello\")");  // Writes to file
     * @endcode
     */
    void set_output_port(OutputPort* port);

    // ========================================================================
    // R7RS Library File Loading
    // ========================================================================

    /**
     * @brief Load a library from a file
     * @param path Path to library file (.scm or .sld)
     * @return True on success, false on error (check get_last_error())
     * @note File should contain a single (define-library ...) form
     * @note Library is parsed, validated, and registered in LibraryRegistry
     *
     * Example:
     * @code
     * if (engine.load_library_file("libs/my-lib.scm")) {
     *     engine.eval_string("(import (my lib))");
     * } else {
     *     std::cerr << engine.get_last_error() << std::endl;
     * }
     * @endcode
     */
    bool load_library_file(const std::string& path);

    /**
     * @brief Add a directory to the library search path
     * @param path Directory path to search for library files
     * @note When auto-loading is enabled, these paths are searched
     * @note Paths are searched in the order they were added
     *
     * Example:
     * @code
     * engine.add_library_search_path("./libs");
     * engine.add_library_search_path("/usr/local/share/scheme");
     * @endcode
     */
    void add_library_search_path(const std::string& path);

    /**
     * @brief Get all library search paths
     * @return Vector of search path strings
     */
    const std::vector<std::string>& get_library_search_paths() const { return library_search_paths_; }

    /**
     * @brief Enable/disable automatic library file loading
     * @param enable If true, attempt to load libraries from files when import fails
     * @note When enabled, failed imports trigger a search in library_search_paths_
     * @note Library name (foo bar baz) maps to files: foo/bar/baz.scm, foo/bar/baz.sld
     *
     * Example:
     * @code
     * engine.add_library_search_path("./libs");
     * engine.set_auto_load_libraries(true);
     * engine.eval_string("(import (my custom lib))");  // Auto-loads libs/my/custom/lib.scm
     * @endcode
     */
    void set_auto_load_libraries(bool enable) { auto_load_libraries_ = enable; }

    /**
     * @brief Check if auto-load is enabled
     * @return True if auto-load is enabled
     */
    bool is_auto_load_enabled() const { return auto_load_libraries_; }

    /**
     * @brief Evaluate a Scheme expression (for library initialization)
     * @param expr Expression to evaluate
     * @param env Environment for evaluation
     * @return Result of evaluation
     * @note This is a low-level evaluation method for internal use
     */
    Object* eval_expr(Object* expr, Environment* env);

    // ========================================================================
    // Primitive Group Management
    // ========================================================================

    /**
     * @brief Install a specific Primitive group into the engine
     * @param name Group name (e.g., "value-ledger", "scenegraph", "my-app")
     * @note Allows applications to opt-in to optional Primitive groups
     * @note Groups must be registered with primitive_registry first
     * @note Silently skips if group is not registered
     *
     * Example - Install optional Value Ledger primitives:
     * @code
     * scheme_engine engine;  // Has core primitives by default
     * engine.install_group("value-ledger");  // Add value ledger primitives
     * engine.eval_string("(value-info 42)");  // Now available
     * @endcode
     *
     * Example - Custom application primitives:
     * @code
     * // In application code:
     * primitive_registry::instance().register_group("my-app", [](Environment* env) {
     *     define(env, "custom-op", [](IEvaluator* eval, Object* args, Environment* env) {
     *         // Custom implementation
     *         return nil;
     *     });
     * });
     *
     * // Later:
     * scheme_engine engine;
     * engine.install_group("my-app");  // Add custom primitives
     * @endcode
     */
    void install_group(const std::string& name);

private:
    // ========================================================================
    // Private Members
    // ========================================================================

    IEvaluator* evaluator_;          ///< Current evaluator (implements IEvaluator interface)
    bool owns_evaluator_;            ///< If true, delete evaluator_ on destruction
    EvaluatorType evaluator_type_;   ///< Type of evaluator (LISP, TOY_VM, BYTECODE)

    Environment* global_env_;         ///< Global environment (holds all bindings)
    scheme_reader reader_;           ///< Parser for Scheme source code

    InputPort* input_port_;          ///< Current input port (default: stdin)
    OutputPort* output_port_;        ///< Current output port (default: stdout or capture buffer)

    bool owns_input_port_;           ///< If true, delete input_port_ on destruction
    bool owns_output_port_;          ///< If true, delete output_port_ on destruction

    bool capture_stdout_;            ///< If true, output goes to captured_output_ instead of stdout
    std::ostringstream captured_output_;  ///< Buffer for captured stdout

    std::string last_error_;         ///< Last error message from eval_string or read_string
    a_exception* last_error_object_; ///< Last error as structured Object (nullptr if none)

    gc_manager* isolated_gc_;        ///< Non-null if this engine owns an isolated GC instance (for test isolation)
    bool owns_isolated_gc_;          ///< If true, shutdown isolated GC on destruction

    std::unique_ptr<RuntimeContext> runtime_context_;  ///< Runtime context (owns GC, ports, ledger, etc.)

    std::vector<std::string> library_search_paths_;  ///< Directories to search for library files
    bool auto_load_libraries_;       ///< If true, auto-load libraries from files when import fails

    // ========================================================================
    // Private Methods
    // ========================================================================

    /**
     * @brief Install all Primitive functions in global environment
     * @note Called by constructors after global_env_ is created
     * @note Registers ~150 built-in functions (+, -, car, cdr, etc.)
     */
    void install_primitives();

    /**
     * @brief Set up default I/O ports (stdin/stdout)
     * @note Called by constructors if no custom ports provided
     */
    void setup_default_ports();

    /**
     * @brief Clean up I/O ports if owned
     * @note Deletes input_port_/output_port_ if owns_*_port_ flags are set
     */
    void cleanup_ports();

    /**
     * @brief Clean up evaluator if owned
     * @note Deletes evaluator_ if owns_evaluator_ is true
     */
    void cleanup_evaluator();

    /**
     * @brief Create evaluator instance by type
     * @param type Evaluator type to instantiate
     * @return Pointer to new evaluator (caller must manage lifetime)
     * @note Factory method for creating evaluators from EvaluatorType enum
     */
    IEvaluator* create_evaluator(EvaluatorType type);
};
