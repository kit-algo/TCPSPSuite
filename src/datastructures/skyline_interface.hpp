#ifndef SKYLINE_INTERFACE_HPP
#define SKYLINE_INTERFACE_HPP

#include "../instance/job.hpp"
#include "../instance/resource.hpp"

#include <dyno.hpp>

#include <cassert>
#include <iterator>
#include <type_traits>
#include <utility>

#include "skyline.hpp"

namespace ds {
using dyno::literals::operator""_s;

struct SkyLineIteratorInterface
    : decltype(::dyno::requires(
          dyno::CopyConstructible{}, dyno::CopyAssignable{},
          dyno::Destructible{}, dyno::EqualityComparable{},
          dyno::DefaultConstructible{},

          "operator++_prefix"_s = ::dyno::method<void()>,
          "operator++_postfix"_s = ::dyno::method<void(int)>,
          "operator--_prefix"_s = ::dyno::method<void()>,
          "operator--_postfix"_s = ::dyno::method<void(int)>,
          "dereference"_s = ::dyno::method<const SkyLineEvent &()>))
{
};

/*
 * SkyLine Interface
 */
struct SkyLineInterface
    : decltype(::dyno::requires(
          dyno::MoveConstructible{},
          "remove_job__job"_s = ::dyno::method<void(const Job &)>,
          "remove_job__jid"_s = ::dyno::method<void(Job::JobId)>,
          "insert_job__job"_s =
              ::dyno::method<void(const Job &, unsigned int pos)>,
          "insert_job__jid"_s =
              ::dyno::method<void(const Job::JobId, unsigned int pos)>,

          "set_pos__job"_s = dyno::method<void(const Job &, unsigned int)>,
          "set_pos__jid"_s = dyno::method<void(Job::JobId, unsigned int)>,

          "get_maximum__unbounded"_s = dyno::method<Resources()>,
          "get_maximum__bounded"_s =
              dyno::method<Resources(unsigned int, unsigned int)>,

          "get_maximum_range__unbounded"_s = dyno::method<MaxRange() const>,
          "get_maximum_range__bounded"_s =
              dyno::method<MaxRange(unsigned int, unsigned int) const>,

          "begin"_s = dyno::method<SkyLineIterator()>,
          "end"_s = dyno::method<SkyLineIterator()>,

          "lower_bound"_s = dyno::method<SkyLineIterator(unsigned int)>,
          "upper_bound"_s = dyno::method<SkyLineIterator(unsigned int)>))
{
};

} // namespace ds

/*
 * Iterator concept map
 *
 * Can't happen at ::ds namespace…
 */
template <class T>
auto const dyno::default_concept_map<ds::SkyLineIteratorInterface, T> =
    dyno::make_concept_map("operator++_prefix"_s = [](T & self) { ++self; },
                           "operator++_postfix"_s =
                               [](T & self, int dummy) {
	                               (void)dummy;
	                               self++;
                               }, // TODO there is an unnecessary copy here
                           "operator--_prefix"_s = [](T & self) { --self; },
                           "operator--_postfix"_s =
                               [](T & self, int dummy) {
	                               (void)dummy;
	                               self--;
                               },
                           "dereference"_s = [](T & self)
                               -> const ds::SkyLineEvent & { return *self; });

/*
 * SkyLine Concept Map
 *
 * This can't happen at ::ds namespace…
 */
template <class T>
auto const
    dyno::default_concept_map<ds::SkyLineInterface, T> = dyno::make_concept_map(
        "remove_job__job"_s = [](T & self,
                                 const Job & job) { self.remove_job(job); },
        "remove_job__jid"_s = [](T & self,
                                 Job::JobId jid) { self.remove_job(jid); },
        "insert_job__job"_s =
            [](T & self, const Job & job, unsigned int pos) {
	            self.insert_job(job, pos);
            },
        "insert_job__jid"_s =
            [](T & self, Job::JobId jid, unsigned int pos) {
	            self.insert_job(jid, pos);
            },
        "set_pos__job"_s = [](T & self, const Job & job,
                              unsigned int pos) { self.set_pos(job, pos); },
        "set_pos__jid"_s = [](T & self, Job::JobId jid,
                              unsigned int pos) { self.set_pos(jid, pos); },
        "get_maximum__unbounded"_s =
            [](T & self) { return self.get_maximum(); },
        "get_maximum__bounded"_s =
            [](T & self, unsigned int lb, unsigned int ub) {
	            return self.get_maximum(lb, ub);
            },
        "get_maximum_range__unbounded"_s =
            [](const T & self) { return self.get_maximum_range(); },
        "get_maximum_range__bounded"_s =
            [](const T & self, unsigned int lb, unsigned int ub) {
	            return self.get_maximum_range(lb, ub);
            },
        "begin"_s = [](T & self) { return ds::SkyLineIterator(self.begin()); },
        "end"_s = [](T & self) { return ds::SkyLineIterator(self.end()); },
        "lower_bound"_s = [](T & self,
                             unsigned int x) { return self.lower_bound(x); },
        "upper_bound"_s = [](T & self,
                             unsigned int x) { return self.upper_bound(x); }

    );

namespace ds {
/*
 * Iterator wrapper class
 */
class SkyLineIterator {
public:
	typedef std::ptrdiff_t difference_type;
	typedef SkyLineEvent value_type;
	typedef const SkyLineEvent & reference;
	typedef const SkyLineEvent * pointer;
	typedef std::forward_iterator_tag iterator_category;

	template <class T>
	SkyLineIterator(T x) : poly_(x)
	{}

	SkyLineIterator(const SkyLineIterator & other) : poly_(other.poly_) {}

	SkyLineIterator &
	operator=(const SkyLineIterator other)
	{
		this->poly_ = other.poly_;
		return *this;
	}

	SkyLineIterator &
	operator++()
	{
		this->poly_.virtual_("operator++_prefix"_s)();
		return *this;
	}

	SkyLineIterator
	operator++(int)
	{
		SkyLineIterator cpy = *this;
		this->poly_.virtual_("operator++_postfix"_s)(0);
		return cpy;
	}

	const SkyLineEvent & operator*()
	{
		return this->poly_.virtual_("dereference"_s)();
	}

	friend bool
	operator==(const SkyLineIterator & lhs, const SkyLineIterator & rhs)
	{
		// TODO should we check this?
		//    assert(lhs.poly_.virtual_("typeid"_s)() ==
		//    rhs.poly_.virtual_("typeid"_s)());
		return lhs.poly_.virtual_("equal"_s)(lhs.poly_, rhs.poly_);
	}

	bool
	operator!=(const SkyLineIterator & other)
	{
		return !(*this == other);
	}

	void
	swap(SkyLineIterator & other)
	{
		std::swap(this->poly_, other.poly_);
	}

	friend void
	swap(SkyLineIterator & a, SkyLineIterator & b)
	{
		a.swap(b);
	}

private:
	dyno::poly<SkyLineIteratorInterface,
	           dyno::local_storage<std::max(
	               sizeof(ArraySkyLineBase<true>::iterator),
	               sizeof(TreeSkyLineBase<true, false>::iterator))>,
	           dyno::vtable<dyno::local<dyno::everything>>>
	    poly_;
};

/*
 * SkyLine wrapper class
 */
class SkyLine {
public:
	// Convert anything that is compatible into a SkyLine
	template <class T>
	SkyLine(T && x) : poly_(std::move(x))
	{}

	void
	remove_job(const Job & job) noexcept
	{
		poly_.virtual_("remove_job__job"_s)(job);
	}

	void
	remove_job(Job::JobId jid) noexcept
	{
		poly_.virtual_("remove_job__jid"_s)(jid);
	}

	void
	insert_job(const Job & job, unsigned int pos) noexcept
	{
		poly_.virtual_("insert_job__job"_s)(job, pos);
	}

	void
	insert_job(Job::JobId jid, unsigned int pos) noexcept
	{
		poly_.virtual_("insert_job__jid"_s)(jid, pos);
	}

	void
	set_pos(const Job & job, unsigned int pos) noexcept
	{
		poly_.virtual_("set_pos__job"_s)(job, pos);
	}

	void
	set_pos(Job::JobId jid, int pos) noexcept
	{
		poly_.virtual_("set_pos__jid"_s)(jid, pos);
	}

	Resources
	get_maximum() noexcept
	{
		return poly_.virtual_("get_maximum__unbounded"_s)();
	}

	Resources
	get_maximum(unsigned int lb, unsigned int ub) noexcept
	{
		return poly_.virtual_("get_maximum__bounded"_s)(lb, ub);
	}

	MaxRange
	get_maximum_range() const noexcept
	{
		return poly_.virtual_("get_maximum_range__unbounded"_s)();
	}

	MaxRange
	get_maximum_range(unsigned int lb, unsigned int ub) const noexcept
	{
		return poly_.virtual_("get_maximum_range__bounded"_s)(lb, ub);
	}

	SkyLineIterator
	begin() noexcept
	{
		return SkyLineIterator(poly_.virtual_("begin"_s)());
	}

	SkyLineIterator
	end() noexcept
	{
		return SkyLineIterator(poly_.virtual_("end"_s)());
	}

	SkyLineIterator
	lower_bound(unsigned int x) noexcept
	{
		return SkyLineIterator(poly_.virtual_("lower_bound"_s)(x));
	}

	SkyLineIterator
	upper_bound(unsigned int x) noexcept
	{
		return SkyLineIterator(poly_.virtual_("upper_bound"_s)(x));
	}

private:
	dyno::poly<
	    SkyLineInterface,
	    dyno::local_storage<std::max(sizeof(ArraySkyLineBase<true>),
	                                 sizeof(TreeSkyLineBase<true, false>))>,
	    dyno::vtable<dyno::local<dyno::everything>>>
	    poly_;
};

} // namespace ds

#endif
