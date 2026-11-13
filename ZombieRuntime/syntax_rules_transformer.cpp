#include "pch.h"
#include "syntax_rules_transformer.h"
#include "atom.h"
#include "cons_cell.h"
#include "Environment.h"
#include "i_evaluator.h"
#include <sstream>

namespace zombie { namespace runtime {

SyntaxRulesTransformer::SyntaxRulesTransformer()
    : m_literals(nil), m_rules(nil), m_gensym_counter(0) {
}

SyntaxRulesTransformer::SyntaxRulesTransformer(Object* literals, Object* rules)
    : m_literals(literals), m_rules(rules), m_gensym_counter(0) {
}

SyntaxRulesTransformer::~SyntaxRulesTransformer() {
}

/**
 * Expand a Macro call by finding the first matching rule and applying it
 */
Object* SyntaxRulesTransformer::expand(Object* input, Environment* env) {
    // Try each rule in order
    for (Object* rule_list = m_rules; rule_list != nil && !rule_list->is_nil(); rule_list = rest(rule_list)) {
        Object* rule = first(rule_list);
        Object* pattern = first(rule);
        Object* template_expr = second(rule);

        // Try to match this pattern
        Object* bindings = match_pattern(pattern, input);

        if (bindings != nullptr) {
            // Match succeeded - expand template with bindings
            Object* expansion = expand_template(template_expr, bindings, env);
            return expansion;
        }
    }

    // No rule matched - return error
    std::ostringstream oss;
    oss << "syntax-rules: no rule matches input";
    return gc_new<a_string>("ERROR: " + oss.str());
}

/**
 * Check if a symbol is in the literals list
 */
bool SyntaxRulesTransformer::is_literal(Object* sym) {
    if (!is_symbol(sym)) return false;

    a_symbol* sym_obj = dynamic_cast<a_symbol*>(sym);
    if (!sym_obj) return false;

    for (Object* lit_list = m_literals; lit_list != nil && !lit_list->is_nil(); lit_list = rest(lit_list)) {
        Object* literal = first(lit_list);
        if (is_symbol(literal)) {
            a_symbol* lit_sym = dynamic_cast<a_symbol*>(literal);
            if (lit_sym && lit_sym->value() == sym_obj->value()) {
                return true;
            }
        }
    }
    return false;
}

/**
 * Match a pattern against input, returning bindings or nullptr
 */
Object* SyntaxRulesTransformer::match_pattern(Object* pattern, Object* input) {
    // Check for ellipsis in pattern
    if (has_ellipsis(pattern)) {
        return match_with_ellipsis(pattern, input);
    }

    // Base case: pattern is nil
    if (pattern == nil || pattern->is_nil()) {
        if (input == nil || input->is_nil()) {
            return nil;  // Empty bindings
        }
        return nullptr;  // No match
    }

    // Pattern is a symbol
    if (pattern->is_atom()) {
        a_symbol* pat_sym = dynamic_cast<a_symbol*>(pattern);
        if (pat_sym) {
            // Check if it's a literal
            if (is_literal(pattern)) {
                // Must match exactly
                a_symbol* input_sym = dynamic_cast<a_symbol*>(input);
                if (input_sym && input_sym->value() == pat_sym->value()) {
                    return nil;  // Match, no bindings
                }
                return nullptr;  // No match
            }

            // It's a pattern variable - bind it to input
            return cons(cons(pattern, input), nil);
        }

        // Other atoms must match exactly
        // This handles numbers, strings, etc.
        return nullptr;  // For now, pattern variables must be symbols
    }

    // Pattern is a list - match recursively
    if (input->is_atom()) {
        return nullptr;  // Can't match list pattern against atom
    }

    Object* pat_first = first(pattern);
    Object* pat_rest = rest(pattern);
    Object* input_first = first(input);
    Object* input_rest = rest(input);

    // Match first element
    Object* first_bindings = match_pattern(pat_first, input_first);
    if (first_bindings == nullptr) {
        return nullptr;  // First element didn't match
    }

    // Match rest
    Object* rest_bindings = match_pattern(pat_rest, input_rest);
    if (rest_bindings == nullptr) {
        return nullptr;  // Rest didn't match
    }

    // Merge bindings
    Object* merged = first_bindings;
    for (Object* b = rest_bindings; b != nil && !b->is_nil(); b = rest(b)) {
        merged = cons(first(b), merged);
    }

    return merged;
}

/**
 * Helper: Check if an Object is the ellipsis symbol
 */
bool SyntaxRulesTransformer::is_ellipsis_symbol(Object* obj) {
    if (!is_symbol(obj)) return false;
    a_symbol* sym = dynamic_cast<a_symbol*>(obj);
    if (!sym) return false;
    std::string val = sym->value();
    return (val == "..." || val == "..");
}

/**
 * Check if pattern contains ellipsis (...)
 */
bool SyntaxRulesTransformer::has_ellipsis(Object* pattern) {
    if (pattern == nil || pattern->is_nil()) {
        return false;
    }
    if (pattern->is_atom()) {
        return false;
    }

    // Check each element in the list
    for (Object* p = pattern; p != nil && !p->is_nil(); p = rest(p)) {
        Object* elem = first(p);

        // Check if next is ellipsis (either as cdr atom or as next element)
        Object* next = rest(p);
        if (next != nil && !next->is_nil()) {
            // Check if cdr is an atom (improper list with ellipsis as cdr)
            if (next->is_atom()) {
                if (is_ellipsis_symbol(next)) {
                    return true;
                }
                // Cdr is atom but not ellipsis - improper list, stop checking
                break;
            }

            // Check if next element is ellipsis (proper list)
            if (!next->is_atom()) {
                Object* next_elem = first(next);
                if (is_ellipsis_symbol(next_elem)) {
                    return true;
                }
            }
        }

        // Recursively check nested lists
        if (!elem->is_atom()) {
            bool nested = has_ellipsis(elem);
            if (nested) {
                return true;
            }
        }
    }

    return false;
}

/**
 * Match pattern with ellipsis support
 */
Object* SyntaxRulesTransformer::match_with_ellipsis(Object* pattern, Object* input) {
    if (pattern == nil || pattern->is_nil()) {
        return (input == nil || input->is_nil()) ? nil : nullptr;
    }

    Object* pat_first = first(pattern);
    Object* pat_rest = rest(pattern);

    // Check if the cdr is ellipsis (improper list) or next element is ellipsis (proper list)
    if (pat_rest != nil && !pat_rest->is_nil()) {
        bool has_ellipsis_here = false;
        Object* rest_of_pattern = nil;

        // Case 1: Improper list (x . ...)
        if (is_ellipsis_symbol(pat_rest)) {
            has_ellipsis_here = true;
            rest_of_pattern = nil;  // Nothing after ellipsis in improper list
        }
        // Case 2: Proper list (x ... rest)
        else if (!pat_rest->is_atom()) {
            Object* second_elem = first(pat_rest);
            if (is_ellipsis_symbol(second_elem)) {
                has_ellipsis_here = true;
                rest_of_pattern = rest(pat_rest);
            }
        }

        if (has_ellipsis_here) {
            // We have (pat_first ... rest_of_pattern)

            // Match zero or more occurrences of pat_first
            Object* ellipsis_bindings = nil;
            Object* current_input = input;

            // Try to match as many times as possible
            while (current_input != nil && !current_input->is_nil()) {
                Object* elem_bindings = match_pattern(pat_first, first(current_input));

                if (elem_bindings == nullptr) {
                    // Can't match more - try matching the rest
                    break;
                }

                // Accumulate bindings
                ellipsis_bindings = cons(elem_bindings, ellipsis_bindings);
                current_input = rest(current_input);
            }

            // Reverse ellipsis_bindings to restore order
            ellipsis_bindings = reverse(ellipsis_bindings);

            // Match the rest of the pattern
            Object* rest_bindings = match_pattern(rest_of_pattern, current_input);
            if (rest_bindings == nullptr) {
                return nullptr;
            }

            // Merge ellipsis bindings into a single binding per variable
            // Each variable in pat_first gets bound to a list of values
            Object* merged_bindings = nil;

            // Collect all pattern variables from pat_first
            Object* pattern_vars = nil;
            for (Object* eb = ellipsis_bindings; eb != nil && !eb->is_nil(); eb = rest(eb)) {
                Object* single_match = first(eb);
                for (Object* b = single_match; b != nil && !b->is_nil(); b = rest(b)) {
                    Object* binding = first(b);
                    Object* var = first(binding);

                    // Add to pattern_vars if not already there
                    bool found = false;
                    for (Object* pv = pattern_vars; pv != nil && !pv->is_nil(); pv = rest(pv)) {
                        if (is_symbol(first(pv)) && is_symbol(var)) {
                            a_symbol* pv_sym = dynamic_cast<a_symbol*>(first(pv));
                            a_symbol* var_sym = dynamic_cast<a_symbol*>(var);
                            if (pv_sym && var_sym && pv_sym->value() == var_sym->value()) {
                                found = true;
                                break;
                            }
                        }
                    }
                    if (!found) {
                        pattern_vars = cons(var, pattern_vars);
                    }
                }
            }

            // For each pattern variable, collect all its values across matches
            for (Object* pv = pattern_vars; pv != nil && !pv->is_nil(); pv = rest(pv)) {
                Object* var = first(pv);
                Object* values = nil;

                for (Object* eb = ellipsis_bindings; eb != nil && !eb->is_nil(); eb = rest(eb)) {
                    Object* single_match = first(eb);
                    Object* value = lookup_binding(var, single_match);
                    values = cons(value, values);
                }

                values = reverse(values);
                merged_bindings = add_binding(var, values, merged_bindings);
            }

            // Add rest_bindings
            for (Object* rb = rest_bindings; rb != nil && !rb->is_nil(); rb = rest(rb)) {
                merged_bindings = cons(first(rb), merged_bindings);
            }

            return merged_bindings;
        }
    }

    // No ellipsis in immediate position - match normally
    if (input->is_atom()) {
        return nullptr;
    }

    Object* first_bindings = match_pattern(pat_first, first(input));
    if (first_bindings == nullptr) {
        return nullptr;
    }

    Object* rest_bindings = match_with_ellipsis(pat_rest, rest(input));
    if (rest_bindings == nullptr) {
        return nullptr;
    }

    // Merge bindings
    Object* merged = first_bindings;
    for (Object* b = rest_bindings; b != nil && !b->is_nil(); b = rest(b)) {
        merged = cons(first(b), merged);
    }

    return merged;
}

/**
 * Expand template using bindings
 */
Object* SyntaxRulesTransformer::expand_template(Object* templ, Object* bindings, Environment* env) {
    // Check for ellipsis in template
    if (!templ->is_atom()) {
        if (has_ellipsis(templ)) {
            return expand_with_ellipsis(templ, bindings);
        }
    }

    // Base case: template is nil
    if (templ == nil || templ->is_nil()) {
        return nil;
    }

    // Template is a symbol - check if it's a pattern variable
    if (templ->is_atom()) {
        a_symbol* templ_sym = dynamic_cast<a_symbol*>(templ);
        if (templ_sym) {
            Object* value = lookup_binding(templ, bindings);
            if (value != nullptr) {
                return value;
            }
        }

        // Not a pattern variable - return as-is (could be a literal or introduced variable)
        return templ;
    }

    // Template is a list - expand recursively
    Object* first_expanded = expand_template(first(templ), bindings, env);
    Object* rest_expanded = expand_template(rest(templ), bindings, env);

    return cons(first_expanded, rest_expanded);
}

/**
 * Expand template with ellipsis
 */
Object* SyntaxRulesTransformer::expand_with_ellipsis(Object* templ, Object* bindings) {
    if (templ == nil || templ->is_nil()) {
        return nil;
    }

    Object* first_elem = first(templ);
    Object* rest_templ = rest(templ);

    // Check if the cdr is ellipsis (improper list) or next element is ellipsis (proper list)
    if (rest_templ != nil && !rest_templ->is_nil()) {
        bool has_ellipsis_here = false;
        Object* rest_of_template = nil;

        // Case 1: Improper list (x . ...)
        if (is_ellipsis_symbol(rest_templ)) {
            has_ellipsis_here = true;
            rest_of_template = nil;
        }
        // Case 2: Proper list (x ... rest)
        else if (!rest_templ->is_atom()) {
            Object* second_elem = first(rest_templ);
            if (is_ellipsis_symbol(second_elem)) {
                has_ellipsis_here = true;
                rest_of_template = rest(rest_templ);
            }
        }

        if (has_ellipsis_here) {
            // We have (elem ... rest)
            // Collect pattern variables that appear in first_elem
            Object* pattern_vars = collect_pattern_vars(first_elem, bindings);

            // Determine iteration count by finding a list binding among pattern_vars
            int iteration_count = 0;
            for (Object* pv = pattern_vars; pv != nil && !pv->is_nil(); pv = rest(pv)) {
                Object* var = first(pv);
                Object* var_binding = lookup_binding(var, bindings);

                if (var_binding && !var_binding->is_atom()) {
                    // Count list length
                    int len = 0;
                    for (Object* vb = var_binding; vb != nil && !vb->is_nil(); vb = rest(vb)) {
                        len++;
                    }
                    if (len > iteration_count) {
                        iteration_count = len;
                    }
                }
            }

            if (iteration_count > 0) {
                // Expand first_elem for each iteration
                Object* expanded = nil;

                for (int i = 0; i < iteration_count; i++) {
                    // Create temporary bindings for this iteration
                    Object* iter_bindings = nil;

                    // Only process pattern variables that appear in first_elem
                    for (Object* pv = pattern_vars; pv != nil && !pv->is_nil(); pv = rest(pv)) {
                        Object* var = first(pv);
                        Object* var_binding = lookup_binding(var, bindings);

                        if (var_binding && !var_binding->is_atom()) {
                            // This is a list binding - get ith element
                            Object* elem = var_binding;
                            for (int j = 0; j < i && elem != nil && !elem->is_nil(); j++) {
                                elem = rest(elem);
                            }

                            if (elem != nil && !elem->is_nil()) {
                                Object* ith_value = first(elem);
                                iter_bindings = add_binding(var, ith_value, iter_bindings);
                            }
                        } else {
                            // Non-list binding (shouldn't happen for pattern_vars in ellipsis context)
                            iter_bindings = add_binding(var, var_binding, iter_bindings);
                        }
                    }

                    // Also include non-pattern-var bindings as-is for context
                    for (Object* b = bindings; b != nil && !b->is_nil(); b = rest(b)) {
                        Object* binding = first(b);
                        Object* var = first(binding);

                        // Check if this var is in pattern_vars
                        bool in_pattern_vars = false;
                        for (Object* pv = pattern_vars; pv != nil && !pv->is_nil(); pv = rest(pv)) {
                            if (is_symbol(var) && is_symbol(first(pv))) {
                                a_symbol* var_sym = dynamic_cast<a_symbol*>(var);
                                a_symbol* pv_sym = dynamic_cast<a_symbol*>(first(pv));
                                if (var_sym && pv_sym && var_sym->value() == pv_sym->value()) {
                                    in_pattern_vars = true;
                                    break;
                                }
                            }
                        }

                        // If not in pattern_vars, add the whole binding
                        if (!in_pattern_vars) {
                            Object* var_binding = rest(binding);
                            iter_bindings = add_binding(var, var_binding, iter_bindings);
                        }
                    }

                    // Expand first_elem with iteration bindings
                    Object* elem_expanded = expand_template(first_elem, iter_bindings, nullptr);
                    expanded = append(expanded, cons(elem_expanded, nil));
                }

                // Expand rest of template
                Object* rest_expanded = expand_with_ellipsis(rest_of_template, bindings);

                return append(expanded, rest_expanded);
            }

            // If no list bindings found, expand to nothing
            Object* rest_expanded = expand_with_ellipsis(rest_of_template, bindings);
            return rest_expanded;
        }
    }

    // No ellipsis - expand normally
    Object* first_expanded = expand_template(first_elem, bindings, nullptr);
    Object* rest_expanded = expand_with_ellipsis(rest_templ, bindings);

    return cons(first_expanded, rest_expanded);
}

/**
 * Lookup a variable in bindings
 */
Object* SyntaxRulesTransformer::lookup_binding(Object* var, Object* bindings) {
    if (!is_symbol(var)) return nullptr;

    a_symbol* var_sym = dynamic_cast<a_symbol*>(var);
    if (!var_sym) return nullptr;

    for (Object* b = bindings; b != nil && !b->is_nil(); b = rest(b)) {
        Object* binding = first(b);
        Object* bound_var = first(binding);

        if (is_symbol(bound_var)) {
            a_symbol* bound_sym = dynamic_cast<a_symbol*>(bound_var);
            if (bound_sym && bound_sym->value() == var_sym->value()) {
                return rest(binding);
            }
        }
    }

    return nullptr;
}

/**
 * Add a binding to bindings list
 */
Object* SyntaxRulesTransformer::add_binding(Object* var, Object* value, Object* bindings) {
    return cons(cons(var, value), bindings);
}

/**
 * Collect all pattern variables mentioned in a template expression
 */
Object* SyntaxRulesTransformer::collect_pattern_vars(Object* templ, Object* bindings) {
    if (templ == nil || templ->is_nil()) {
        return nil;
    }

    // If it's a symbol, check if it has a binding
    if (templ->is_atom()) {
        if (is_symbol(templ)) {
            if (lookup_binding(templ, bindings) != nullptr) {
                return cons(templ, nil);
            }
        }
        return nil;
    }

    // It's a list - collect from all elements (excluding ellipsis symbols)
    Object* vars = nil;
    for (Object* t = templ; t != nil && !t->is_nil(); t = rest(t)) {
        Object* elem = first(t);

        // Skip ellipsis symbols
        if (is_ellipsis_symbol(elem)) {
            continue;
        }

        // Collect from this element
        Object* elem_vars = collect_pattern_vars(elem, bindings);

        // Merge into vars (avoiding duplicates)
        for (Object* ev = elem_vars; ev != nil && !ev->is_nil(); ev = rest(ev)) {
            Object* var = first(ev);

            // Check if already in vars
            bool found = false;
            for (Object* v = vars; v != nil && !v->is_nil(); v = rest(v)) {
                if (is_symbol(var) && is_symbol(first(v))) {
                    a_symbol* var_sym = dynamic_cast<a_symbol*>(var);
                    a_symbol* v_sym = dynamic_cast<a_symbol*>(first(v));
                    if (var_sym && v_sym && var_sym->value() == v_sym->value()) {
                        found = true;
                        break;
                    }
                }
            }

            if (!found) {
                vars = cons(var, vars);
            }
        }
    }

    return vars;
}

} } // namespace zombie::runtime
