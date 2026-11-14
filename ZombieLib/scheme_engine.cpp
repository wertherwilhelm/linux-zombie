#include "pch.h"
#include "scheme_engine.h"
#include "runtime_context.h"
#include "Lisp.h"
#include "toy_vm.h"
#include "bytecode_vm.h"
#include "primitive_base.h"
#include "primitive.h"  // For Primitive::install_primitives()
#include "Base.h"
#include "atom.h"
#include "cons_cell.h"
#include "constants.h"
#include "library_parser.h"
#include "library_registry.h"
#include "r7rs_stdlib.h"
#include "gc_context.h"
#include "InputPort.h"
#include "OutputPort.h"
#include "a_exception.h"
#include <iostream>
#include <fstream>


scheme_engine::scheme_engine(bool use_isolated_gc)
    : evaluator_(nullptr)
    , owns_evaluator_(true)
    , evaluator_type_(EvaluatorType::LISP)
    , global_env_(nullptr)
    , input_port_(nullptr)
    , output_port_(nullptr)
    , owns_input_port_(false)
    , owns_output_port_(false)
    , capture_stdout_(false)
    , last_error_object_(nullptr)
    , owns_isolated_gc_(false)
    , runtime_context_(nullptr)
    , auto_load_libraries_(false)
{
    if (use_isolated_gc) {
        throw std::runtime_error("Isolated GC not yet supported");
    }

    // Create runtime context (initializes GC, constants, ports, ledger)
    runtime_context_ = RuntimeContext::create();

    // Set this context as current for the lifetime of this engine
    RuntimeContext::set_current(runtime_context_.get());

    // Create global environment through GC and register as persistent root
    // This is critical: Environment contains bindings that must survive GC
    global_env_ = gc_new<Environment>();
    if (auto* gc = gc_context::current()) {
        gc->register_persistent_root(reinterpret_cast<gc_object**>(&global_env_));
    }

    // Install Primitive functions
    install_primitives();

    // Bootstrap R7RS standard libraries
    r7rs::bootstrap_standard_libraries(global_env_);

    // Create evaluator (default: Lisp)
    evaluator_ = create_evaluator(EvaluatorType::LISP);

    // Set up default I/O ports
    setup_default_ports();
}

scheme_engine::scheme_engine(EvaluatorType type, bool use_isolated_gc)
    : evaluator_(nullptr)
    , owns_evaluator_(true)
    , evaluator_type_(type)
    , global_env_(nullptr)
    , input_port_(nullptr)
    , output_port_(nullptr)
    , owns_input_port_(false)
    , owns_output_port_(false)
    , capture_stdout_(false)
    , last_error_object_(nullptr)
    , owns_isolated_gc_(false)
    , runtime_context_(nullptr)
    , auto_load_libraries_(false)
{
    if (use_isolated_gc) {
        throw std::runtime_error("Isolated GC not yet supported");
    }

    // Create runtime context (initializes GC, constants, ports, ledger)
    runtime_context_ = RuntimeContext::create();

    // Set this context as current for the lifetime of this engine
    RuntimeContext::set_current(runtime_context_.get());

    // Create global environment through GC and register as persistent root
    // This is critical: Environment contains bindings that must survive GC
    global_env_ = gc_new<Environment>();
    if (auto* gc = gc_context::current()) {
        gc->register_persistent_root(reinterpret_cast<gc_object**>(&global_env_));
    }

    // Install Primitive functions
    install_primitives();

    // Bootstrap R7RS standard libraries
    r7rs::bootstrap_standard_libraries(global_env_);

    // Create evaluator by type
    evaluator_ = create_evaluator(type);

    // Set up default I/O ports
    setup_default_ports();
}

scheme_engine::scheme_engine(IEvaluator* evaluator, bool take_ownership)
    : evaluator_(evaluator)
    , owns_evaluator_(take_ownership)
    , evaluator_type_(EvaluatorType::LISP)  // Unknown type
    , global_env_(nullptr)
    , input_port_(nullptr)
    , output_port_(nullptr)
    , owns_input_port_(false)
    , owns_output_port_(false)
    , capture_stdout_(false)
    , last_error_object_(nullptr)
    , owns_isolated_gc_(false)
    , runtime_context_(nullptr)
    , auto_load_libraries_(false)
{
    if (!evaluator_) {
        throw std::invalid_argument("scheme_engine: evaluator cannot be null");
    }

    // Create runtime context (initializes GC, constants, ports, ledger)
    runtime_context_ = RuntimeContext::create();

    // Set this context as current for the lifetime of this engine
    RuntimeContext::set_current(runtime_context_.get());

    // Use evaluator's environment
    global_env_ = evaluator_->get_environment();

    // Install Primitive functions
    install_primitives();

    // Bootstrap R7RS standard libraries
    r7rs::bootstrap_standard_libraries(global_env_);

    // Set up default I/O ports
    setup_default_ports();
}

scheme_engine::~scheme_engine() {
    cleanup_ports();
    cleanup_evaluator();
    // Note: global_env_ will be cleaned up by GC
}

void scheme_engine::install_primitives() {
    // Install all R5RS primitives using new inheritance-based classes
    // This calls PrimitiveBooleans::install(), PrimitiveLists::install(), etc.
    zombie::runtime::Primitive::install_primitives(global_env_);

    // Note: The registry system is still available for optional extension groups
    // (register with primitive_registry, then install_group() to add them).
}

void scheme_engine::install_group(const std::string& name) {
    // Install a specific Primitive group into the global environment
    zombie::runtime::primitive_registry::instance().install_groups(global_env_, {name});
}

void scheme_engine::setup_default_ports() {
    // Default to stdin/stdout
    input_port_ = new InputPort(std::cin);
    output_port_ = new OutputPort(std::cout);
    owns_input_port_ = true;
    owns_output_port_ = true;

    // Sync with global ports used by primitives (display, read, etc.)
    // This ensures primitives use the same ports as the engine
    stdin_port = input_port_;
    stdout_port = output_port_;

    // Sync with RuntimeContext
    if (runtime_context_) {
        runtime_context_->set_input_port(input_port_);
        runtime_context_->set_output_port(output_port_);
    }
}

void scheme_engine::cleanup_ports() {
    if (owns_input_port_ && input_port_) {
        delete input_port_;
        input_port_ = nullptr;
    }
    if (owns_output_port_ && output_port_) {
        delete output_port_;
        output_port_ = nullptr;
    }
}

void scheme_engine::cleanup_evaluator() {
    if (owns_evaluator_ && evaluator_) {
        delete evaluator_;
        evaluator_ = nullptr;
    }
}

IEvaluator* scheme_engine::create_evaluator(EvaluatorType type) {
    IEvaluator* eval = nullptr;

    switch (type) {
        case EvaluatorType::LISP: {
            Lisp* lisp_eval = new Lisp();
            lisp_eval->set_environment(global_env_);
            eval = lisp_eval;
            break;
        }
        case EvaluatorType::TOY_VM: {
            toy::VM* vm_eval = new toy::VM(global_env_, false);  // false = no debug
            eval = vm_eval;
            break;
        }
        case EvaluatorType::BYTECODE: {
            zombie::bytecode::BytecodeVM* bc_eval = new zombie::bytecode::BytecodeVM(global_env_, false);  // false = no debug
            eval = bc_eval;
            break;
        }
        default:
            throw std::invalid_argument("Unknown evaluator type");
    }

    return eval;
}

// ============================================================================
// R7RS Library Support (REPL Integration) - Forward Declarations and Helpers
// ============================================================================

// Check if an expression is a symbol with a given name
static bool is_symbol_named(Object* obj, const std::string& name) {
    if (!obj || obj->get_type() != static_cast<int>(Type::SYMBOL)) {
        return false;
    }
    a_symbol* sym = static_cast<a_symbol*>(obj);
    return sym->getName() == name;
}

// Check if expression is (define-library ...)
static bool is_define_library_form(Object* expr) {
    if (!expr || !expr->pair()) {
        return false;
    }
    return is_symbol_named(car(expr), "define-library");
}

// Check if expression is (import ...)
static bool is_import_form(Object* expr) {
    if (!expr || !expr->pair()) {
        return false;
    }
    return is_symbol_named(car(expr), "import");
}

// Handle (define-library ...) form
// Parses, initializes, and registers the library, returns true on success
static bool handle_define_library(Object* expr, Environment* env,
                                    scheme_engine* engine, std::string& error_out) {
    try {
        r7rs::LibraryParser parser;
        auto lib = parser.parse_library_definition(expr);

        // Initialize the library (sets up environment and imports)
        if (!lib->initialize(env, error_out)) {
            return false;
        }

        // Evaluate begin blocks to define the library's functions
        auto eval_func = [engine](Object* form, Environment* env_arg) -> Object* {
            return engine->eval_expr(form, env_arg);
        };

        if (!lib->evaluate_begin_blocks(eval_func, error_out)) {
            return false;
        }

        // Validate the library (check exports are defined)
        std::string validation_errors;
        if (!lib->validate(validation_errors)) {
            error_out = "Library validation failed: " + validation_errors;
            return false;
        }

        // Register the library
        r7rs::LibraryRegistry& registry = r7rs::LibraryRegistry::instance();
        registry.register_library(std::move(lib));

        return true;
    } catch (const std::exception& e) {
        error_out = std::string("Failed to define library: ") + e.what();
        return false;
    }
}

// Helper: Convert library name to file path
// (foo bar baz) -> "foo/bar/baz"
static std::string library_name_to_path(const r7rs::LibraryName& name) {
    if (name.empty()) {
        return "";
    }

    std::string path = name[0];
    for (size_t i = 1; i < name.size(); ++i) {
        path += "/" + name[i];
    }
    return path;
}

// Helper: Try to find library file in search paths
// Returns empty string if not found
static std::string find_library_file(const r7rs::LibraryName& lib_name,
                                      const std::vector<std::string>& search_paths) {
    std::string relative_path = library_name_to_path(lib_name);

    // Try each search path
    for (const auto& search_dir : search_paths) {
        // Try .scm extension
        std::string scm_path = search_dir + "/" + relative_path + ".scm";
        std::ifstream scm_file(scm_path);
        if (scm_file.good()) {
            scm_file.close();
            return scm_path;
        }

        // Try .sld extension (R7RS standard library definition)
        std::string sld_path = search_dir + "/" + relative_path + ".sld";
        std::ifstream sld_file(sld_path);
        if (sld_file.good()) {
            sld_file.close();
            return sld_path;
        }
    }

    return "";  // Not found
}

// Handle (import ...) form
// Imports symbols from libraries into the given environment, returns true on success
static bool handle_import(Object* expr, Environment* env, scheme_engine* engine, std::string& error_out) {
    try {
        r7rs::LibraryParser parser;

        // Parse the import declaration (returns vector of ImportSpecs)
        auto import_specs = parser.parse_import_declaration(expr);

        r7rs::LibraryRegistry& registry = r7rs::LibraryRegistry::instance();

        // Process each import set
        for (const auto& import_spec : import_specs) {
            // Get the imported library
            r7rs::Library* imported_lib = registry.get_library(import_spec.library);

            // If not found and auto-load is enabled, try to load from file
            if (!imported_lib && engine && engine->is_auto_load_enabled()) {
                std::string lib_file = find_library_file(import_spec.library,
                                                          engine->get_library_search_paths());
                if (!lib_file.empty()) {
                    // Try to load the library from file
                    if (engine->load_library_file(lib_file)) {
                        // Try to get the library again
                        imported_lib = registry.get_library(import_spec.library);
                    }
                }
            }

            if (!imported_lib) {
                error_out = "Cannot find library " + r7rs::library_name_to_string(import_spec.library);
                return false;
            }

            // Make sure the library is initialized
            if (!imported_lib->is_initialized()) {
                std::string init_error;
                // Pass nullptr as parent for REPL imports (global environment)
                if (!imported_lib->initialize(nullptr, init_error)) {
                    error_out = "Failed to initialize library " +
                                r7rs::library_name_to_string(import_spec.library) + ": " + init_error;
                    return false;
                }
            }

            // Get all exported symbols from the imported library
            const std::vector<r7rs::ExportSpec>& exports = imported_lib->exports();

            // Import based on import type
            switch (import_spec.type) {
            case r7rs::ImportSpec::SIMPLE: {
                // Import all exported symbols
                for (const auto& exp : exports) {
                    Object* value = imported_lib->get_symbol(exp.internal_name);
                    if (value) {
                        env->define(exp.external_name.c_str(), value);
                    }
                }
                break;
            }

            case r7rs::ImportSpec::ONLY: {
                // Import only specified symbols
                for (const auto& exp : exports) {
                    if (import_spec.symbols.find(exp.external_name) != import_spec.symbols.end()) {
                        Object* value = imported_lib->get_symbol(exp.internal_name);
                        if (value) {
                            env->define(exp.external_name.c_str(), value);
                        }
                    }
                }
                break;
            }

            case r7rs::ImportSpec::EXCEPT: {
                // Import all except specified symbols
                for (const auto& exp : exports) {
                    if (import_spec.symbols.find(exp.external_name) == import_spec.symbols.end()) {
                        Object* value = imported_lib->get_symbol(exp.internal_name);
                        if (value) {
                            env->define(exp.external_name.c_str(), value);
                        }
                    }
                }
                break;
            }

            case r7rs::ImportSpec::PREFIX: {
                // Import all with prefix
                for (const auto& exp : exports) {
                    Object* value = imported_lib->get_symbol(exp.internal_name);
                    if (value) {
                        std::string prefixed_name = import_spec.prefix + exp.external_name;
                        env->define(prefixed_name.c_str(), value);
                    }
                }
                break;
            }

            case r7rs::ImportSpec::RENAME: {
                // Import with renames
                for (const auto& exp : exports) {
                    Object* value = imported_lib->get_symbol(exp.internal_name);
                    if (value) {
                        // Check if this symbol should be renamed
                        auto rename_it = import_spec.renames.find(exp.external_name);
                        if (rename_it != import_spec.renames.end()) {
                            // Use renamed name
                            env->define(rename_it->second.c_str(), value);
                        } else {
                            // Use original name
                            env->define(exp.external_name.c_str(), value);
                        }
                    }
                }
                break;
            }
            }
        }

        return true;
    } catch (const std::exception& e) {
        error_out = std::string("Failed to import: ") + e.what();
        return false;
    }
}

void* scheme_engine::eval_string(std::string_view source) {
    last_error_.clear();
    last_error_object_ = nullptr;

    // Parse the source code (convert string_view to string)
    Object* expr_list = reader_.read_program_from_string(std::string(source));

    if (!expr_list) {
        last_error_ = "Failed to parse source code";
        return nullptr;
    }

    // Check for parse error
    a_string* err = dynamic_cast<a_string*>(expr_list);
    if (err && err->value().find("ERROR:") == 0) {
        last_error_ = err->value();
        return nullptr;
    }

    // GC SAFETY: root the parsed program for the whole evaluation. Without
    // this, any collection triggered mid-eval (allocation threshold, or the
    // gc-collect primitive) frees the AST we are still walking.
    gc_root<Object> guard_program(expr_list);

    // Evaluate all expressions, return last result
    Object* result = nullptr;
    gc_root<Object> guard_result(result);
    Object* current = expr_list;

    while (current && !is_nil(current)) {
        if (!current->pair()) {
            last_error_ = "Invalid expression list";
            return nullptr;
        }

        Object* expr = car(current);

        // Check for R7RS library forms (define-library, import)
        if (is_define_library_form(expr)) {
            // Handle (define-library ...) form
            std::string error;
            if (!handle_define_library(expr, global_env_, this, error)) {
                last_error_ = error;
                return nullptr;
            }
            // define-library doesn't return a value, use nil
            result = nil;
        } else if (is_import_form(expr)) {
            // Handle (import ...) form
            std::string error;
            if (!handle_import(expr, global_env_, this, error)) {
                last_error_ = error;
                return nullptr;
            }
            // import doesn't return a value, use nil
            result = nil;
        } else {
            // Normal expression - evaluate it
            try {
                result = evaluator_->eval(expr, global_env_);
            } catch (const scheme_error& e) {
                // Structured Scheme error - preserve exception Object if available
                last_error_ = std::string("Evaluation error: ") + e.message();
                last_error_object_ = e.exception();
                return nullptr;
            } catch (const std::exception& e) {
                // Generic C++ exception - string error only
                last_error_ = std::string("Evaluation error: ") + e.what();
                return nullptr;
            } catch (...) {
                last_error_ = "Unknown evaluation error";
                return nullptr;
            }
        }

        // Keep the most recent result alive across the next iteration's
        // evaluation (which may allocate and collect)
        guard_result = gc_root<Object>(result);

        current = cdr(current);
    }

    return result;
}

Object* scheme_engine::eval_expr(Object* expr) {
    last_error_.clear();
    last_error_object_ = nullptr;

    if (!expr) {
        last_error_ = "Cannot evaluate null expression";
        return nullptr;
    }

    try {
        return evaluator_->eval(expr, global_env_);
    } catch (const scheme_error& e) {
        // Structured Scheme error - preserve exception Object if available
        last_error_ = std::string("Evaluation error: ") + e.message();
        last_error_object_ = e.exception();
        return nullptr;
    } catch (const std::exception& e) {
        // Generic C++ exception - string error only
        last_error_ = std::string("Evaluation error: ") + e.what();
        return nullptr;
    } catch (...) {
        last_error_ = "Unknown evaluation error";
        return nullptr;
    }
}

Object* scheme_engine::read_string(const std::string& source) {
    last_error_.clear();

    Object* expr = reader_.read_from_string(source);

    if (!expr) {
        last_error_ = "Failed to parse source code";
        return nullptr;
    }

    // Check for parse error
    a_string* err = dynamic_cast<a_string*>(expr);
    if (err && err->value().find("ERROR:") == 0) {
        last_error_ = err->value();
        return nullptr;
    }

    return expr;
}

Object* scheme_engine::read_program(const std::string& source) {
    last_error_.clear();

    Object* program = reader_.read_program_from_string(source);

    if (!program) {
        last_error_ = "Failed to parse program";
        return nullptr;
    }

    // Check for parse error
    a_string* err = dynamic_cast<a_string*>(program);
    if (err && err->value().find("ERROR:") == 0) {
        last_error_ = err->value();
        return nullptr;
    }

    return program;
}

std::string scheme_engine::print_object(Object* obj) {
    if (!obj) {
        return "nil";
    }

    std::ostringstream oss;
    obj->print(oss);
    return oss.str();
}

void scheme_engine::reset() {
    // Clean up old evaluator if we own it
    cleanup_evaluator();

    // Unregister old environment root and create new one through GC
    // Note: We reuse the same slot (&global_env_) so we don't need to unregister
    // the old root - the slot will be updated in place
    global_env_ = gc_new<Environment>();
    // The slot is already registered as a persistent root from constructor

    // Reinstall primitives
    install_primitives();

    // Re-bootstrap R7RS standard libraries (must match constructor behavior)
    r7rs::bootstrap_standard_libraries(global_env_);

    // Recreate evaluator with same type
    evaluator_ = create_evaluator(evaluator_type_);
    owns_evaluator_ = true;

    // Clear errors
    last_error_.clear();

    // Clear captured output
    captured_output_.str("");
    captured_output_.clear();
}

std::string scheme_engine::print_value(void* value) {
    auto* obj = static_cast<Object*>(value);
    return print_object(obj);
}

void scheme_engine::warmup_prelude() {
    // Default implementation does nothing
    // Subclasses can override to load standard library
    // Example:
    // eval_string("(define (caar x) (car (car x)))");
    // eval_string("(define (cadr x) (car (cdr x)))");
    // etc.
}

void scheme_engine::set_input_port(InputPort* port) {
    if (owns_input_port_ && input_port_) {
        delete input_port_;
    }
    input_port_ = port;
    owns_input_port_ = false;

    // Sync with global port used by primitives (read, read-char, etc.)
    stdin_port = port;

    // Sync with RuntimeContext
    if (runtime_context_) {
        runtime_context_->set_input_port(port);
    }
}

void scheme_engine::set_output_port(OutputPort* port) {
    if (owns_output_port_ && output_port_) {
        delete output_port_;
    }
    output_port_ = port;
    owns_output_port_ = false;

    // Sync with global port used by primitives (display, write, etc.)
    stdout_port = port;

    // Sync with RuntimeContext
    if (runtime_context_) {
        runtime_context_->set_output_port(port);
    }
}

void scheme_engine::set_capture_stdout(bool enable) {
    capture_stdout_ = enable;
    if (enable) {
        captured_output_.str("");
        captured_output_.clear();

        // Replace output port with one that writes to our buffer
        if (owns_output_port_ && output_port_) {
            delete output_port_;
        }
        output_port_ = new OutputPort(captured_output_);
        owns_output_port_ = true;

        // Sync with global port so primitives (display, write) use capture buffer
        stdout_port = output_port_;

        // Sync with RuntimeContext
        if (runtime_context_) {
            runtime_context_->set_output_port(output_port_);
        }
    } else {
        // Restore default stdout
        if (owns_output_port_ && output_port_) {
            delete output_port_;
        }
        output_port_ = new OutputPort(std::cout);
        owns_output_port_ = true;

        // Sync with global port
        stdout_port = output_port_;

        // Sync with RuntimeContext
        if (runtime_context_) {
            runtime_context_->set_output_port(output_port_);
        }
    }
}

std::string scheme_engine::take_captured_stdout() {
    std::string result = captured_output_.str();
    captured_output_.str("");
    captured_output_.clear();
    return result;
}

void scheme_engine::clear_captured_stdout() {
    captured_output_.str("");
    captured_output_.clear();
}

Object* scheme_engine::eval_expr(Object* expr, Environment* env) {
    return evaluator_->eval(expr, env);
}

// ============================================================================
// R7RS Library File Loading
// ============================================================================

bool scheme_engine::load_library_file(const std::string& path) {
    last_error_.clear();

    // Try to read the file
    std::ifstream file(path);
    if (!file.is_open()) {
        last_error_ = "Failed to open library file: " + path;
        return false;
    }

    // Read entire file
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    if (content.empty()) {
        last_error_ = "Library file is empty: " + path;
        return false;
    }

    // Parse the file
    Object* expr = reader_.read_from_string(content);
    if (!expr) {
        last_error_ = "Failed to parse library file: " + path;
        return false;
    }

    // Check if it's a define-library form
    if (!is_define_library_form(expr)) {
        last_error_ = "File does not contain a define-library form: " + path;
        return false;
    }

    // Handle the library definition
    std::string error;
    if (!handle_define_library(expr, global_env_, this, error)) {
        last_error_ = "Failed to load library from " + path + ": " + error;
        return false;
    }

    return true;
}

void scheme_engine::add_library_search_path(const std::string& path) {
    library_search_paths_.push_back(path);
}
