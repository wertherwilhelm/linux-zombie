/**
 * @file ledger.cpp
 * @brief Implementation of Value Ledger methods requiring Object.h
 */

#include "pch.h"
#include "ledger.h"
#include "Object.h"  // Full definition needed for method calls

// Global ledger instance
Ledger* g_ledger = nullptr;

// ============================================================================
// Ledger Entry Management Methods
// ============================================================================

LedgerEntry& Ledger::ensure_entry(Object* obj, const std::string& origin, const std::string& traits) {
    if (!obj) {
        throw std::runtime_error("Cannot create ledger entry for null Object");
    }

    // Assign ValueId if needed
    if (!obj->has_value_id()) {
        obj->set_value_id(next_value_id_++);
    }

    ValueId id = obj->value_id();

    // Create entry if it doesn't exist (preserve existing!)
    auto it = provenance_.find(id);
    if (it == provenance_.end()) {
        // First time seeing this value - create entry
        provenance_[id] = LedgerEntry(BackendTag::Scheme, origin, traits);
    }

    return provenance_[id];
}

const LedgerEntry* Ledger::lookup_entry(Object* obj) const {
    if (!obj || !obj->has_value_id()) {
        return nullptr;
    }

    auto it = provenance_.find(obj->value_id());
    return it == provenance_.end() ? nullptr : &it->second;
}

void Ledger::forget(Object* obj) {
    if (obj && obj->has_value_id()) {
        provenance_.erase(obj->value_id());
    }
}
