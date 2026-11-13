#pragma once

/**
 * @file cons_list.h
 * @brief Lightweight wrapper around cons-cell lists with STL-style iteration.
 */

#include "atom.h"
#include "cons_cell.h"
#include "constants.h"
#include "Base.h"
#include <vector>
#include <stack>

namespace zombie { namespace runtime {

/**
 * @brief Wrapper for Scheme lists implemented with cons cells.
 *
 * Provides minimal list construction and iteration helpers without owning
 * the underlying cons cells.
 */
class cons_list {
public:
    /**
     * @brief Forward iterator over a linear cons list.
     */
    struct Iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using ValueType = Object;
        using pointer = Object*;
        using reference = Object&;

        Iterator(pointer ptr) : m_ptr(ptr) {}

        reference operator*() const { return *car(m_ptr); }
        pointer operator->() { return m_ptr; }
        Iterator& operator++() { m_ptr = (cdr(m_ptr)); return *this; }
        Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }
        friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_ptr == b.m_ptr; };
        friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_ptr != b.m_ptr; };

    private:
        pointer m_ptr;
    };

    /**
     * @brief Experimental iterator for nested list traversal.
     *
     * Note: Currently advances like a linear iterator (cdr-only).
     */
    struct TreeIterator
    {
        typedef std::pair<int, Object&> list_item; // return the Object and its depth
        typedef std::stack<Object*> parent_list;

        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using ValueType = Object;
        using pointer = Object*;
        using reference = Object&;

        TreeIterator(pointer ptr) : m_ptr(ptr) {}

        reference operator*() const { return *car(m_ptr); }
        pointer operator->() { return m_ptr; }
        TreeIterator& operator++() 
        { 
            m_ptr = (cdr(m_ptr)); return *this; 
            // going to have to use a stack initially with nil
            //  when descending push pointer onto the stack
            //  reach end pop
            //  when you get to nil and pop nil you are done
        }
        TreeIterator operator++(int) { TreeIterator tmp = *this; ++(*this); return tmp; }
        friend bool operator== (const TreeIterator& a, const TreeIterator& b) { return a.m_ptr == b.m_ptr; };
        friend bool operator!= (const TreeIterator& a, const TreeIterator& b) { return a.m_ptr != b.m_ptr; };

    private:
        pointer m_ptr;
    };

    /**
     * @brief Get iterator to first element.
     */
    Iterator begin() { return Iterator(m_ptr); }

    /**
     * @brief Get iterator past the end (nil).
     */
    Iterator end() { return Iterator(nil); }

    /**
     * @brief Construct an empty list.
     */
    cons_list();

    /**
     * @brief Construct list from vector of elements.
     * @param vec Elements to append in order.
     */
    cons_list(std::vector<Object*> vec);

    /**
     * @brief Append an element to the end of the list.
     * @param o Element to append.
     */
    void push_back(Object* o);

    /**
     * @brief Get underlying list pointer.
     */
    Object* get_list() { return m_ptr; }

    /**
     * @brief Return car of the list.
     */
    Object* lcar();

    /**
     * @brief Return cdr of the list.
     */
    Object* lcdr();

private:
    Object* m_ptr;
};

} } // namespace zombie::runtime
