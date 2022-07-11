/*
 * Copyright 2022 Pyarelal Knowles
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */
/*
 * Based on code from
 * https://functionalcpp.wordpress.com/2013/08/05/function-traits/
 *
 * Author: gnzlbg
 */

#pragma once

namespace psp {

template <class F> struct function_traits;

// function pointer
template <class R, class... Args>
struct function_traits<R (*)(Args...)> : public function_traits<R(Args...)> {};

// member function pointer
template <class C, class R, class... Args>
struct function_traits<R (C::*)(Args...)>
    : public function_traits<R(C &, Args...)> {};

// member object pointer
template <class C, class R>
struct function_traits<R(C::*)> : public function_traits<R(C &)> {};

// const member function pointer
template <class C, class R, class... Args>
struct function_traits<R (C::*)(Args...) const>
    : public function_traits<R(C &, Args...)> {};

// functor
template <class F> struct function_traits {
private:
    using call_type = function_traits<decltype(&F::operator())>;

public:
    using return_type = typename call_type::return_type;

    // O.o
    template <typename R, typename As> static As pro_args(std::function<R(As)>);

    template <typename R, typename... As>
    static std::tuple<As...> pro_args(std::function<R(As...)>);

    using arg_type = decltype(pro_args(std::function{std::declval<F>()}));
};

template <class F> struct function_traits<F &> : public function_traits<F> {};

template <class F> struct function_traits<F &&> : public function_traits<F> {};

template <class R, class... Args> struct function_traits<R(Args...)> {
    using return_type = R;
    using arg_type = std::tuple<Args...>;
};

template <class R, class Arg> struct function_traits<R(Arg)> {
    using return_type = R;
    using arg_type = Arg;
};

} // namespace psp
