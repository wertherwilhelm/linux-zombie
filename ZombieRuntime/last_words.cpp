/**
 * @file last_words.cpp
 * @brief Implementation of the GC graveyard (see last_words.h).
 */

#include "pch.h"
#include "last_words.h"
#include "Object.h"
#include "atom.h"
#include "Base.h"
#include "gc_interface.h"
#include "gc_context.h"

#include <deque>
#include <unordered_map>
#include <sstream>

namespace zombie { namespace runtime { namespace necromancy {

namespace {

std::deque<std::string> g_graves;                              // oldest..newest
std::unordered_map<const gc_object*, std::string> g_epitaphs;  // custom last words
uint64_t g_death_toll = 0;

/// Shallow description of a dying object. Must not follow references:
/// neighbors may already be reclaimed in the same sweep.
std::string describe(gc_object* g) {
    Object* obj = dynamic_cast<Object*>(g);
    if (!obj) {
        return "#<gc-object>";
    }

    switch (static_cast<Type>(obj->get_type())) {
        // Self-contained atoms: printing touches no other GC object
        case Type::INT:
        case Type::LONG:
        case Type::DOUBLE:
        case Type::CHAR:
        case Type::BOOL:
        case Type::STRING:
        case Type::SYMBOL: {
            std::ostringstream oss;
            // Quote strings so a dying source literal's natural epitaph can
            // never be mistaken for someone's custom last words
            bool is_str = obj->get_type() == static_cast<int>(Type::STRING);
            if (is_str) oss << '"';
            obj->print(oss);
            if (is_str) oss << '"';
            std::string s = oss.str();
            if (s.size() > 48) {
                s.resize(45);
                s += "...";
            }
            return s;
        }
        case Type::VECTOR:     return "#<vector>";
        case Type::BYTEVECTOR: return "#<bytevector>";
        case Type::RATIONAL:   return "#<rational>";
        case Type::COMPLEX:    return "#<complex>";
        case Type::EXCEPTION:  return "#<error-object>";
        case Type::CPP_OBJECT: return "#<cpp-object>";
        default:
            break;
    }

    if (obj->pair()) {
        return "#<pair>";
    }

    if (Base* b = dynamic_cast<Base*>(obj)) {
        switch (static_cast<Base::atype>(b->instance_of())) {
            case Base::atype::CLOSURE:   return "#<closure>";
            case Base::atype::MACRO:     return "#<macro>";
            case Base::atype::PROCEDURE:
            case Base::atype::PRIMITIVE: return "#<procedure>";
            default: break;
        }
    }

    return "#<object>";
}

} // anonymous namespace

void record_death(gc_object* obj) {
    ++g_death_toll;

    std::string line;
    auto it = g_epitaphs.find(obj);
    if (it != g_epitaphs.end()) {
        line = it->second;
        g_epitaphs.erase(it);
    } else {
        line = describe(obj);
    }

    g_graves.push_back(std::move(line));
    while (g_graves.size() > GRAVEYARD_CAPACITY) {
        g_graves.pop_front();
    }
}

void install_obituary_hook() {
    if (IGarbageCollector* gc = gc_context::current()) {
        gc->set_obituary_hook(&record_death);
    }
}

void set_epitaph(Object* obj, const std::string& words) {
    if (obj) {
        g_epitaphs[obj] = words;
    }
}

std::vector<std::string> recent_graves() {
    return std::vector<std::string>(g_graves.rbegin(), g_graves.rend());
}

uint64_t death_toll() {
    return g_death_toll;
}

std::string hold_seance() {
    std::ostringstream oss;
    if (g_graves.empty()) {
        oss << "The graveyard is empty. The living are too careful.";
        return oss.str();
    }

    oss << "The candles gutter. The veil thins. "
        << g_death_toll << (g_death_toll == 1 ? " soul has" : " souls have")
        << " crossed over.\n";
    oss << "The most recent dead speak:\n";

    size_t shown = 0;
    for (auto it = g_graves.rbegin(); it != g_graves.rend() && shown < 8; ++it, ++shown) {
        oss << "  * " << *it << "\n";
    }
    if (g_graves.size() > shown) {
        oss << "  ...and " << (g_graves.size() - shown) << " fainter voices.";
    } else {
        oss << "The silence returns.";
    }
    return oss.str();
}

} } } // namespace zombie::runtime::necromancy
