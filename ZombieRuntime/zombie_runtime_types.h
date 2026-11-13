#pragma once

/**
 * @file zombie_runtime_types.h
 * @brief Central header for namespace bridging
 *
 * This header brings all commonly used types from zombie::runtime
 * into the global namespace. Include this in files that need access
 * to the runtime types without explicit namespace qualification.
 */

#include "Object.h"
#include "Base.h"
#include "atom.h"
#include "cons_cell.h"
#include "a_vector.h"
#include "closure.h"
#include "procedure.h"
#include "primitive.h"
#include "Environment.h"
#include "InputPort.h"
#include "OutputPort.h"
#include "FileInputPort.h"
#include "FileOutputPort.h"
#include "StringInputPort.h"
#include "StringOutputPort.h"
#include "scheme_reader.h"
#include "a_bytevector.h"
#include "a_exception.h"
#include "a_parameter.h"
#include "expander.h"
#include "type_object.h"
#include "values_object.h"
#include "continuation.h"
#include "i_debugger.h"
#include "constants.h"
#include "lisp.h"
#include "i_eval_engine.h"

// Bring all common types from zombie::runtime into global namespace
using zombie::runtime::Object;
using zombie::runtime::Base;
using zombie::runtime::Environment;
using zombie::runtime::Procedure;
using zombie::runtime::Primitive;
using zombie::runtime::primitive_registry;
using zombie::runtime::Closure;
using zombie::runtime::IEvaluator;
using zombie::runtime::Type;

// Atom types
using zombie::runtime::a_int;
using zombie::runtime::a_long;
using zombie::runtime::a_double;
using zombie::runtime::a_float;
using zombie::runtime::a_string;
using zombie::runtime::a_symbol;
using zombie::runtime::a_char;
using zombie::runtime::a_bool;
template<typename T> using atom = zombie::runtime::atom<T>;

// Cell types
using zombie::runtime::ConsCell;
using zombie::runtime::a_vector;

// Constants
using zombie::runtime::nil;
using zombie::runtime::t;
using zombie::runtime::false_obj;
using zombie::runtime::empty_list;
using zombie::runtime::is_empty_list;
using zombie::runtime::init_lisp_constants;

// Functions
using zombie::runtime::is_nil;
using zombie::runtime::is_string;
using zombie::runtime::is_symbol;
using zombie::runtime::is_boolean;
using zombie::runtime::car;
using zombie::runtime::cdr;
using zombie::runtime::first;
using zombie::runtime::rest;
using zombie::runtime::cons;
using zombie::runtime::set_first;
using zombie::runtime::set_rest;
using zombie::runtime::length;
using zombie::runtime::truth;
using zombie::runtime::error;
using zombie::runtime::eq;
using zombie::runtime::eqv;
using zombie::runtime::equal;
using zombie::runtime::is_list;
using zombie::runtime::intern_symbol;
using zombie::runtime::warn;

// Port types
using zombie::runtime::InputPort;
using zombie::runtime::OutputPort;
using zombie::runtime::FileInputPort;
using zombie::runtime::FileOutputPort;
using zombie::runtime::StringInputPort;
using zombie::runtime::StringOutputPort;
using zombie::runtime::stdin_port;
using zombie::runtime::stdout_port;
using zombie::runtime::scheme_reader;

// Additional types
using zombie::runtime::a_bytevector;
using zombie::runtime::a_exception;
using zombie::runtime::a_parameter;
using zombie::runtime::scheme_error;
using zombie::runtime::Expander;
using zombie::runtime::TypeObject;
using zombie::runtime::ValuesObject;

// Continuation and debugger types
using zombie::runtime::Continuation;
using zombie::runtime::DebugContext;
using zombie::runtime::DebugFrame;
using zombie::runtime::Breakpoint;
using zombie::runtime::IDebugger;
using zombie::runtime::NullDebugger;

// Evaluators
using zombie::runtime::Lisp;
using zombie::runtime::IEvalEngine;

// Additional types from primitive.h
template<typename F> using TemplatedProcedure = zombie::runtime::TemplatedProcedure<F>;

// Backward compatibility aliases (lowercase names used by GeneticProgramming)
using object = zombie::runtime::Object;
using lisp = zombie::runtime::Lisp;
using enviroment = zombie::runtime::Environment;
using procedure = zombie::runtime::Procedure;
using primitive = zombie::runtime::Primitive;
using closure = zombie::runtime::Closure;
