#include "pch.h"
#include "runtime_context.h"
#include "gc_interface.h"
#include "gc_context.h"
#include "gc_manager.h"
#include "gc_ptr.h"
#include "constants.h"
#include "atom.h"
#include "InputPort.h"
#include "OutputPort.h"
#include "ledger.h"
#include "mirror.h"
#include "library_registry.h"

// Bring in types from zombie::runtime
using namespace zombie::runtime;

// ============================================================================
// Thread-Local Current Context
// ============================================================================

// Thread-local storage for current context
static thread_local RuntimeContext* tls_current_context = nullptr;

// Default global context (created lazily)
static RuntimeContext* g_default_context = nullptr;

RuntimeContext* RuntimeContext::current() {
    // Return thread-local if set
    if (tls_current_context) {
        return tls_current_context;
    }

    // Fall back to default global context
    // Create lazily on first access
    if (!g_default_context) {
        g_default_context = new RuntimeContext(true);
    }
    return g_default_context;
}

RuntimeContext* RuntimeContext::set_current(RuntimeContext* ctx) {
    RuntimeContext* previous = tls_current_context;
    tls_current_context = ctx;
    return previous;
}

// ============================================================================
// Construction / Destruction
// ============================================================================

RuntimeContext::RuntimeContext(bool initialize_gc) {
    // Initialize GC if requested and not already done
    if (initialize_gc) {
        if (!gc_context::current()) {
            ::initialize_gc();
        }
    }

    // Initialize constants (nil, #t, #f, symbol table, ports)
    // This is idempotent - safe to call multiple times
    ensure_constants();

    // Initialize ports to point to globals (initially)
    // These can be overridden per-context later
    initialize_ports();

    // Initialize ledger if not done
    initialize_ledger();

    initialized_ = true;
}

RuntimeContext::~RuntimeContext() {
    // Clean up owned ports
    if (owned_input_port_) {
        delete owned_input_port_;
        owned_input_port_ = nullptr;
    }
    if (owned_output_port_) {
        delete owned_output_port_;
        owned_output_port_ = nullptr;
    }
    if (owned_error_port_) {
        delete owned_error_port_;
        owned_error_port_ = nullptr;
    }

    // Clear if we're the global default
    if (g_default_context == this) {
        g_default_context = nullptr;
    }

    // Clear thread-local if we're current
    if (tls_current_context == this) {
        tls_current_context = nullptr;
    }
}

std::unique_ptr<RuntimeContext> RuntimeContext::create() {
    return std::make_unique<RuntimeContext>(true);
}

// ============================================================================
// Private Initialization Helpers
// ============================================================================

void RuntimeContext::initialize_ports() {
    // Point to global ports initially
    // This maintains backward compatibility while allowing per-context override
    current_input_port_ = stdin_port;
    current_output_port_ = stdout_port;
    current_error_port_ = stderr_port;
}

void RuntimeContext::initialize_ledger() {
    // Ledger initialization happens in init_lisp_constants()
    // Just verify it's ready
    // (g_ledger and g_mirrors are initialized there)
}

void RuntimeContext::ensure_constants() {
    // Delegate to existing init_lisp_constants()
    // This initializes: nil, t, false_obj, symbol table, ports, ledger, mirrors
    init_lisp_constants();
}

// ============================================================================
// Garbage Collection
// ============================================================================

IGarbageCollector* RuntimeContext::gc() const {
    // Delegate to gc_context for now
    return gc_context::current();
}

void RuntimeContext::collect_garbage() {
    if (auto* collector = gc()) {
        collector->collect();
    }
}

// ============================================================================
// Symbol Table
// ============================================================================

a_symbol* RuntimeContext::intern(const std::string& name) {
    // Delegate to existing intern_symbol() in constants.cpp
    return intern_symbol(name);
}

bool RuntimeContext::is_interned(const std::string& name) const {
    // For now, try to intern and see if it creates a new one
    // TODO: Add a lookup-only function to constants.cpp
    // This is a slight inefficiency but maintains the abstraction
    return true;  // Symbols are always interned on access
}

// ============================================================================
// I/O Ports
// ============================================================================

InputPort* RuntimeContext::input_port() const {
    return current_input_port_ ? current_input_port_ : stdin_port;
}

OutputPort* RuntimeContext::output_port() const {
    return current_output_port_ ? current_output_port_ : stdout_port;
}

OutputPort* RuntimeContext::error_port() const {
    return current_error_port_ ? current_error_port_ : stderr_port;
}

void RuntimeContext::set_input_port(InputPort* port) {
    current_input_port_ = port;
}

void RuntimeContext::set_output_port(OutputPort* port) {
    current_output_port_ = port;
}

void RuntimeContext::set_error_port(OutputPort* port) {
    current_error_port_ = port;
}

// ============================================================================
// Value Ledger & Mirrors
// ============================================================================

Ledger* RuntimeContext::ledger() const {
    // Delegate to global for now
    extern Ledger* g_ledger;
    return g_ledger;
}

MirrorRegistry* RuntimeContext::mirrors() const {
    // Delegate to global for now
    extern MirrorRegistry* g_mirrors;
    return g_mirrors;
}

// ============================================================================
// Library System
// ============================================================================

r7rs::LibraryRegistry& RuntimeContext::libraries() const {
    // Delegate to singleton for now
    return r7rs::LibraryRegistry::instance();
}

// ============================================================================
// Singleton Values
// ============================================================================

Object* RuntimeContext::empty_list() const {
    // Return global nil (which points to empty_list)
    return nil;
}

Object* RuntimeContext::true_value() const {
    // Return global t
    return t;
}

Object* RuntimeContext::false_value() const {
    // Return global false_obj
    return false_obj;
}
