/*
 * Copyright 2022 Pyarelal Knowles
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include "function_traits.hpp"
#include <cstddef>
#include <optional>

namespace psp {

template<class Value>
class indexed_value
{
public:
    using size_type = std::size_t;

    template <class ValueFwd>
    indexed_value(size_type index, size_type step, ValueFwd &&value)
        : index(index), step(step), value(value) {}

    size_type index;
    size_type step;
    Value value;
};

template<class Iterator>
class indexed_iterator
{
public:
    using iterator_category = typename Iterator::iterator_category;
    using difference_type = typename Iterator::iterator_category;
    using value_type = indexed_value<typename Iterator::value_type>;
    using pointer = value_type *;
    using const_pointer = const value_type *;
    using reference = value_type &;
    using const_reference = const value_type &;
    using size_type = std::size_t;

    indexed_iterator(indexed_iterator& other) = default;

    template<class IteratorFwd>
    indexed_iterator(Iterator& iterator) : m_index(0), m_step(0), m_iterator(iterator) {}
    indexed_iterator(Iterator&& iterator) : m_index(0), m_step(0), m_iterator(std::move(iterator)) {}

    const_reference operator*() const { update(); return m_value.operator*(); }
    reference operator*() { update(); return m_value.operator*(); }
    const_pointer operator->() const { update(); return m_value.operator->(); }
    pointer operator->() { update(); return m_value.operator->(); }
    difference_type operator-(const indexed_iterator &other) const {
        return m_iterator.operator-(other);
    }
    indexed_iterator &operator++() {
        ++m_index;
        m_iterator.operator++();
        return *this;
    }
    indexed_iterator operator++(int) {
        return indexed_iterator(m_index++, m_step, m_iterator.operator++(0));
    }
    bool operator==(const indexed_iterator &other) const {
        return m_iterator == other.m_iterator;
    };
    bool operator!=(const indexed_iterator &other) const {
        return !(*this == other);
    };

    size_type index() const { return m_index; }
    size_type step() const { return m_step; }

private:
    indexed_iterator(size_type index, size_type step,
                     Iterator &&iterator)
        : m_index(index), m_step(step), m_iterator(iterator) {
    }

    void update() const
    {
        m_value.emplace(m_index, m_step, m_iterator.operator*());
    }

    size_type m_index;
    size_type m_step;
    mutable std::optional<value_type> m_value;
    Iterator m_iterator;
};

template<class Func>
class indexed_function {
    static_assert(std::is_same_v<std::size_t, std::tuple_element_t<0, typename function_traits<Func>::arg_types>>, "first parameter must be 'size_t index'");
    static_assert(std::is_same_v<std::size_t, std::tuple_element_t<1, typename function_traits<Func>::arg_types>>, "second parameter must be 'size_t step'");
    using input_value_type = indexed_value<std::tuple_element_t<2, typename function_traits<Func>::arg_types>>;
    using output_value_type = indexed_value<typename function_traits<Func>::return_type>;
public:
  indexed_function(Func& func) : m_func(func) {}
  output_value_type operator()(input_value_type& value){
    return output_value_type(value.index, value.step, m_func(std::as_const(value.index), std::as_const(value.step), value.value));
  }
private:
  Func m_func;
};

} // namespace psp
