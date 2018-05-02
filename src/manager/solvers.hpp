//
// Created by lukas on 24.04.18.
//

#ifndef TCPSPSUITE_SOLVERS_HPP
#define TCPSPSUITE_SOLVERS_HPP

#include <type_traits>

#include <boost/hana.hpp>
#include <boost/hana/set.hpp>
#include <boost/hana/assert.hpp>
namespace hana = boost::hana;

/* ==============================================================================
 *    plugins.hpp - providing methods for plugin registration
 * ============================================================================== */

namespace solvers {

template <class>
struct sfinae_true : std::true_type
{
};

/* This template will be specialized by every plugin with its own integer.
 * The operator() of the respective struct should return a hana::set of all the
 * classes registered so far. */
template <unsigned int>
struct registry_hook;

/* Methods to test whether a registry_hook with a specified number is already
 * defined via SFINAE. */
template <class Dummy, unsigned int testN>
constexpr auto
test(int) -> sfinae_true<decltype(registry_hook<testN>{})>;

template <class Dummy, unsigned int testN>
constexpr auto
test(long) -> std::false_type;

/* Returns the smallest number N for which registry_hook<N> is not defined yet.
 * A per-plugin unique dummy class is necessary s.t. this struct is re-instantiated for
 * every plugin.
 */
template <class Dummy, unsigned int testN = 0>
constexpr unsigned int
get_free_N()
{
	/* The 'constexpr' is essential here. Otherwise, the template in the
	 * else-branch must be instantiated even if the condition evaluates to true,
	 * leading to infinite recursion. */
	if constexpr (std::is_same<decltype(test<Dummy, testN>(0)), std::false_type>::value) {
		return testN;
	} else {
		return get_free_N<Dummy, testN + 1>();
	}
}

/* Helper struct / method to chain together the registered classes */
template <class ClassToRegister, unsigned int myN>
struct register_class
{
	/* General case: myN > 0. I.e., other classes have already been registered.
	 * We recursively get the set of these classes and append our own. */
	auto
	operator()()
	{
		return hana::insert(registry_hook<myN - 1>{}(), hana::type_c < ClassToRegister > );
	}
};

template <class ClassToRegister>
struct register_class<ClassToRegister, 0>
{
	/* Special case: myN == 0. No other classes have been registered. Create a new
	 * set. */
	auto
	operator()()
	{
		return hana::make<hana::set_tag>(hana::type_c < ClassToRegister > );
	}
};


} // namespace solvers

#endif //TCPSPSUITE_SOLVERS_HPP
