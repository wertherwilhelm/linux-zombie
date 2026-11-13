#pragma once

/**
 * @file primitive_necromancy.h
 * @brief GC graveyard primitives (the "Last Words" system, see last_words.h)
 *
 * Scheme-facing necromancy:
 *   (graveyard)            ; list of recent epitaphs, newest first
 *   (epitaph obj "text")   ; custom last words, spoken when obj is collected
 *   (death-toll)           ; total objects reclaimed since startup
 *   (seance)               ; theatrical report of recent deaths
 *   (gc-collect)           ; force a collection; returns objects reclaimed
 *
 * The costume is gothic; the body is GC observability.
 */

#include "../primitive_base.h"
#include "../Environment.h"

class PrimitiveNecromancy : public PrimitiveBase {
public:
    enum ID {
        GRAVEYARD,   ///< graveyard
        EPITAPH,     ///< epitaph
        DEATHTOLL,   ///< death-toll
        SEANCE,      ///< seance
        GCCOLLECT    ///< gc-collect
    };

    PrimitiveNecromancy(ID id, int minArgs, int maxArgs);

    Object* apply(IEvaluator* eval, Object* args, Environment* env) override;

    /**
     * @brief Install all necromancy primitives into environment
     */
    static void install(Environment* env);

private:
    ID m_id;
    std::string m_name;
};
