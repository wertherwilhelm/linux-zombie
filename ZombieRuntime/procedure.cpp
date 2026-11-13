#include "pch.h"
// Procedure.cpp: implementation of the Procedure class.
//
//////////////////////////////////////////////////////////////////////

#include "procedure.h"
#include "Lisp.h"
#include "gc_manager.h"

namespace zombie { namespace runtime {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Procedure::Procedure()
{

}

Procedure::~Procedure()
{

}

void Procedure::trace_references(Tracer& tracer)
{
    // Base Procedure class has no Object references
    // Derived classes like Closure will override this
}

} } // namespace zombie::runtime
