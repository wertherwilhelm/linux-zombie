#include "pch.h"
#include "cons_list.h"
#include "Base.h"

namespace zombie { namespace runtime {

cons_list::cons_list()
{
    m_ptr = nullptr;
}

cons_list::cons_list(std::vector<Object*> vec)
{
    m_ptr = nullptr;
    m_ptr = make_list<Object*>(vec);
}

void cons_list::push_back(Object* o)
{
    if (m_ptr == nullptr) {
        m_ptr = cons(o, nil);
    } else {
        m_ptr = append(m_ptr, o);
    }
}

Object* cons_list::lcar()
{
    return car(m_ptr);
}

Object* cons_list::lcdr()
{
    return cdr(m_ptr);
}

} } // namespace zombie::runtime