//
// Created by lukas on 10.04.18.
//

#ifndef TCPSPSUITE_TEMPLATE_MAGIC_HPP
#define TCPSPSUITE_TEMPLATE_MAGIC_HPP

#include <type_traits>

namespace utilities {

/*
 * Allows your classes to optionally contain members, without re-writing the
 * whole class in a partial specialization.
 */
template<class T, bool enabled>
struct OptionalMember {} ;

template<class T>
struct OptionalMember<T, false> {
	/* Accept anything inside the constructor */
	template<class ... Ts>
	OptionalMember(Ts ...) {}
};

template<class T>
struct OptionalMember<T, true> : public T
{
public:
        using T::T;
};

	/*
	 * Allows switching of member types based on a constexpr
	 * bool expression.
	 */
	template<class TrueType, class FalseType, bool b>
	struct ConditionalMember {};

	template<class TrueType, class FalseType>
	struct ConditionalMember<TrueType, FalseType, true> : public TrueType
	{
	public:
		using TrueType::TrueType;
	};

	template<class TrueType, class FalseType>
	struct ConditionalMember<TrueType, FalseType, false> : public FalseType
	{
	public:
		using FalseType::FalseType;
	};


/*
 * This is inspired by
 *
 * https://stackoverflow.com/questions/34099597/check-if-a-type-is-passed-in-variadic-template-parameter-pack
 *
 * and should be converted to std::disjunction as soon as C++17 becomes available on reasonable
 * compilers.
 */
template<typename QueryT>
constexpr bool pack_contains() {
        return false;
}

// Forward
template<typename QueryT, typename First, typename ...Rest>
constexpr bool pack_contains();

template<typename QueryT, bool found, typename ...Rest>
constexpr typename std::enable_if<found, bool>::type pack_contains_forward() {
        return true;
}

template<typename QueryT, bool found, typename ...Rest>
constexpr typename std::enable_if<! found, bool>::type pack_contains_forward() {
        return pack_contains<QueryT, Rest...>();
}

template<typename QueryT, typename First, typename ...Rest>
constexpr bool pack_contains() {
        return pack_contains_forward<QueryT, std::is_same<QueryT, First>::value, Rest...>();
}

/*
 * Merges two parameter Packs
 */


/*
 * Generic class to contain a template parameter pack
 */
template<class ...> struct pack {};

}

#endif //TCPSPSUITE_TEMPLATE_MAGIC_HPP
