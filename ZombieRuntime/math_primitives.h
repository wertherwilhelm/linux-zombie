#pragma once 

#include "procedure.h"
#include "number.h"
#include "Environment.h"
#include "Lisp.h"

//class Lisp;
//class Environment;

class AddFunction : public Procedure {
public:
    Object* clone() const { return new AddFunction(); }
    void print(std::ostream& os) const { os << "+"; }

    Object* apply(Object* interp, Object* args) {
        int sum = 0;
        while (args != nullptr) {
            Object* arg = dynamic_cast<Lisp*>(interp)->eval(car(args));
            if (arg->get_type() != 2) {
                throw std::runtime_error("Invalid argument type for +, expected a number");
            }
            sum += dynamic_cast<a_double*>(arg)->get_value();
            args = dynamic_cast<ConsCell*>(args->cdr());
        }
        return new lisp_number(sum);
    }
};

class SubFunction : public Function {
public:
    Object* clone() const { return new SubFunction(); }
    void print(std::ostream& os) const { os << "-"; }

    Object* apply(ConsCell* args, Environment& env) {
        if (args == nullptr) {
            throw std::runtime_error("Invalid number of arguments for -, expected at least 1");
        }
        Object* first_arg = eval(args->car(), env);
        if (first_arg->get_type() != 2) {
            throw std::runtime_error("Invalid argument type for -, expected a number");
        }
        int diff = dynamic_cast<Number*>(first_arg)->get_value();
        args = dynamic_cast<ConsCell*>(args->cdr());

        if (args == nullptr) {
            return new Number(-diff);
        }

        while (args != nullptr) {
            Object* arg = eval(args->car(), env);
            if (arg->get_type() != 2) {
                throw std::runtime_error("Invalid argument type for -, expected a number");
            }
            diff -= dynamic_cast<Number*>(arg)->get_value();
            args = dynamic_cast<ConsCell*>(args->cdr());
        }
        return new Number(diff);
    }
};
