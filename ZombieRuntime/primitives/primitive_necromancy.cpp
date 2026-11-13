/**
 * @file primitive_necromancy.cpp
 * @brief Implementation of GC graveyard primitives
 */

#include "pch.h"
#include "../pch.h"
#include "primitive_necromancy.h"
#include "../last_words.h"
#include "../atom.h"
#include "../Base.h"
#include "../cons_cell.h"
#include "../constants.h"
#include "../numeric_runtime.h"
#include "gc_interface.h"
#include "gc_context.h"
#include "gc_ptr.h"

namespace necro = zombie::runtime::necromancy;

// ============================================================================
// Constructor
// ============================================================================

PrimitiveNecromancy::PrimitiveNecromancy(ID id, int minArgs, int maxArgs)
    : PrimitiveBase(minArgs, maxArgs), m_id(id) {
    switch (id) {
        case GRAVEYARD:  m_name = "graveyard"; break;
        case EPITAPH:    m_name = "epitaph"; break;
        case DEATHTOLL:  m_name = "death-toll"; break;
        case SEANCE:     m_name = "seance"; break;
        case GCCOLLECT:  m_name = "gc-collect"; break;
    }
}

// ============================================================================
// Primitive Application
// ============================================================================

Object* PrimitiveNecromancy::apply(IEvaluator* /*eval*/, Object* args, Environment* /*env*/) {
    if (!check_arity(args)) {
        return arity_error("");
    }

    switch (m_id) {
        case GRAVEYARD: {
            // (graveyard) - list of recent epitaph strings, newest first
            std::vector<std::string> graves = necro::recent_graves();
            Object* result = nil;
            gc_root<Object> guard_result(result);
            for (auto it = graves.rbegin(); it != graves.rend(); ++it) {
                a_string* line = gc_new<a_string>(*it);
                gc_root<Object> guard_line(line);
                result = cons(line, result);
                guard_result = gc_root<Object>(result);
            }
            return result;
        }

        case EPITAPH: {
            // (epitaph obj "text") - custom last words for obj, spoken when
            // the collector takes it. Returns obj. Does not keep obj alive.
            Object* obj = first(args);
            Object* words = second(args);
            if (words->get_type() != static_cast<int>(Type::STRING)) {
                return error("epitaph: last words must be a string");
            }
            necro::set_epitaph(obj, static_cast<a_string*>(words)->value());
            return obj;
        }

        case DEATHTOLL:
            // (death-toll) - total objects reclaimed since startup
            return numeric::make_integer(static_cast<int64_t>(necro::death_toll()));

        case SEANCE:
            // (seance) - theatrical report of recent deaths
            return gc_new<a_string>(necro::hold_seance());

        case GCCOLLECT: {
            // (gc-collect) - force a collection; returns objects reclaimed
            IGarbageCollector* gc = gc_context::current();
            if (!gc) {
                return error("gc-collect: no garbage collector active");
            }
            uint64_t before = necro::death_toll();
            gc->collect();
            return numeric::make_integer(static_cast<int64_t>(necro::death_toll() - before));
        }

        default:
            return error("Unknown necromancy Primitive");
    }
}

// ============================================================================
// Installation
// ============================================================================

void PrimitiveNecromancy::install(Environment* env) {
    // GC Safety: protect each name string before allocating the primitive
    #define DEF(name, ...) do { \
        gc_root<a_string> _n(gc_new<a_string>(name)); \
        env->define(_n.get(), gc_new<PrimitiveNecromancy>(__VA_ARGS__)); \
    } while(0)

    DEF("graveyard",  GRAVEYARD, 0, 0);
    DEF("epitaph",    EPITAPH, 2, 2);
    DEF("death-toll", DEATHTOLL, 0, 0);
    DEF("seance",     SEANCE, 0, 0);
    DEF("gc-collect", GCCOLLECT, 0, 0);

    #undef DEF
}
