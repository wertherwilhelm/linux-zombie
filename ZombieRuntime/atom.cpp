#include "pch.h"
#include "atom.h"
#include "Object.h"

namespace zombie { namespace runtime {

bool eql(const Object* o1, const Object* o2)
{
    return false;
}

bool is_int(const Object* o1)
{
    return ((ValueType*)o1)->get_type() == static_cast<int>(Type::INT);
}

bool is_double(const Object* o1)
{
    return ((ValueType*)o1)->get_type() == static_cast<int>(Type::DOUBLE);
}

bool is_string(const Object* o1)
{
    return ((ValueType*)o1)->get_type() == static_cast<int>(Type::STRING);
}

bool is_symbol(const Object* o1)
{
    return ((ValueType*)o1)->get_type() == static_cast<int>(Type::SYMBOL);
}

bool is_char(const Object* o1)
{
    return ((ValueType*)o1)->get_type() == static_cast<int>(Type::CHAR);
}

bool is_bool(const Object* o1)
{
    return ((ValueType*)o1)->get_type() == static_cast<int>(Type::BOOL);
}

bool is_number(const Object* o1)
{
    if (!o1) return false;
    int t = ((ValueType*)o1)->get_type();
    return t == static_cast<int>(Type::INT) || t == static_cast<int>(Type::LONG) || t == static_cast<int>(Type::DOUBLE) ||
           t == static_cast<int>(Type::RATIONAL) || t == static_cast<int>(Type::COMPLEX);
}

std::ostream& operator<<(std::ostream& os, const Type& type)
{
    switch (type)
    {
        case Type::NOTYPE: os << "NoType"; break;
        case Type::CHAR: os << "Char"; break;
        case Type::BOOL: os << "Bool"; break;
        case Type::INT: os << "Int"; break;
        case Type::STRING: os << "String"; break;
        case Type::LONG: os << "Long"; break;
        case Type::DOUBLE: os << "Double"; break;
        case Type::NUMBER: os << "Number"; break;
        default: os << "Unk"; break;
    }
    return os;
}

} } // namespace zombie::runtime