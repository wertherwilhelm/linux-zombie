/**
 * @file last_words.h
 * @brief The graveyard: GC obituaries for Zombie Scheme.
 *
 * A language named Zombie Scheme should not let its dead vanish in silence.
 * When the collector reclaims an object, this module records a short
 * epitaph in a bounded ring buffer. Scheme code can inspect recent deaths
 * ((graveyard)), give an object custom last words ((epitaph obj "text")),
 * count the fallen ((death-toll)), or hold a (seance).
 *
 * Beyond the theater this is genuine GC observability: "what did the
 * collector just kill?" is a real debugging question, and the epitaph of
 * an object that died too young is a use-after-free caught in the act.
 *
 * Safety contract (enforced here, required by the obituary hook):
 * - record_death runs DURING the sweep: the dying object is still valid,
 *   but objects it references may already be gone. Epitaphs are therefore
 *   strictly shallow — self-contained atoms print their value, everything
 *   else gets a type label.
 * - No GC allocation inside the hook. Only C++ strings.
 */
#pragma once

#include <string>
#include <vector>
#include <cstdint>

class gc_object;
class Object;

namespace zombie { namespace runtime { namespace necromancy {

/// Capacity of the graveyard ring buffer (oldest graves are lost to time).
constexpr size_t GRAVEYARD_CAPACITY = 64;

/// Wire record_death into the GC as its obituary hook. Idempotent.
void install_obituary_hook();

/// The obituary hook itself (called by the GC for every reclaimed object).
void record_death(gc_object* obj);

/// Give an object custom last words, spoken when it is collected.
/// Does not root the object — marked objects still die on schedule.
void set_epitaph(Object* obj, const std::string& words);

/// Recent epitaphs, newest first. At most GRAVEYARD_CAPACITY entries.
std::vector<std::string> recent_graves();

/// Total number of objects that have passed through the obituary hook.
uint64_t death_toll();

/// A theatrical, human-readable account of recent deaths.
std::string hold_seance();

} } } // namespace zombie::runtime::necromancy
