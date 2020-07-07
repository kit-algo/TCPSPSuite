//
// Created by lukas on 06.12.17.
//

#ifndef TCPSPSUITE_SKYLINE_HPP
#define TCPSPSUITE_SKYLINE_HPP

#include "../contrib/intervaltree/src/dynamic_segment_tree.hpp" // for Combine...
#include "../instance/job.hpp"                                  // for Job
#include "../instance/resource.hpp"                             // for Resources
#include "../util/template_magic.hpp" // for Optiona...

#include <boost/container/flat_set.hpp> // for flat_set
#include <boost/hana.hpp>               // for hana
#include <iterator>                     // for forward...
#include <stddef.h>                     // for size_t
#include <utility>                      // for move, pair
#include <vector>                       // for vector
class Instance;

namespace hana = boost::hana;

namespace ds {
using MaxRange = std::pair<unsigned int, unsigned int>;

class SkyLineEvent {
public:
	SkyLineEvent(const Instance * instance) : usage(instance) {}
	SkyLineEvent(){};

	// Current resource usage *after* the event
	Resources usage;

	// where does the event happen
	unsigned int where;

	// Does a job start here?
	bool start;

	bool
	operator==(const SkyLineEvent & other) const noexcept
	{
		return (this->usage == other.usage) && (this->where == other.where) &&
		       (this->start == other.start);
	}
};

// Forward
class SkyLineIterator;

/*
 * ArraySkyLineBase
 */
template <bool support_iteration>
class ArraySkyLineBase {
public:
	class iterator {
	public:
		typedef ptrdiff_t difference_type;
		typedef SkyLineEvent value_type;
		typedef const SkyLineEvent & reference;
		typedef const SkyLineEvent * pointer;
		typedef std::input_iterator_tag iterator_category;

		iterator() noexcept;
		iterator(const iterator & other) noexcept;
		iterator(iterator && other) noexcept;
		iterator(ArraySkyLineBase * sl, size_t time_index) noexcept;

		iterator & operator=(const iterator & other) noexcept;
		iterator & operator=(iterator && other) noexcept;

		bool operator==(const iterator & other) const noexcept;
		bool operator!=(const iterator & other) const noexcept;

		iterator & operator++() noexcept;
		iterator operator++(int) noexcept;
		iterator & operator+=(size_t steps) noexcept;
		iterator operator+(size_t steps) const noexcept;

		iterator & operator--() noexcept;
		iterator operator--(int) noexcept;
		iterator & operator-=(size_t steps) noexcept;
		iterator operator-(size_t steps) const noexcept;

		reference operator*() const noexcept;
		pointer operator->() const noexcept;

		/* Conversion to generic iterator */
		// operator SkyLineIterator() noexcept;

	private:
		void update_ev() noexcept;
		void update_resource_amount(bool went_forward) noexcept;

		ArraySkyLineBase * sl;
		size_t time_index;
		struct event_compare
		{
			bool
			operator()(const std::pair<bool, Job::JobId> & lhs,
			           const std::pair<bool, Job::JobId> & rhs) const noexcept
			{
				if (lhs.first != rhs.first) {
					return !lhs.first;
				} else {
					return lhs.second < rhs.second;
				}
			}
		};
		typename boost::container::flat_set<std::pair<bool, Job::JobId>,
		                                    event_compare>::iterator event_it;

		SkyLineEvent ev;
	};

public:
	ArraySkyLineBase(const Instance * instance);

	void remove_job(const Job & job) noexcept;
	void remove_job(Job::JobId jid) noexcept;
	void insert_job(const Job & job, unsigned int pos) noexcept;
	void insert_job(Job::JobId jid, unsigned int pos) noexcept;
	void set_pos(const Job & job, unsigned int pos) noexcept;
	void set_pos(Job::JobId jid, unsigned int pos) noexcept;

	Resources get_maximum() noexcept;
	Resources get_maximum(unsigned int l, unsigned int r) noexcept;

	using MaxRange = std::pair<unsigned int, unsigned int>;

	MaxRange get_maximum_range() const noexcept;
	MaxRange get_maximum_range(unsigned int l, unsigned int r) const noexcept;

	iterator begin() noexcept;
	iterator end() noexcept;

	iterator lower_bound(unsigned int x) noexcept;
	iterator upper_bound(unsigned int x) noexcept;

private:
	const Instance * const instance;

	// TODO outer vector should be a small_vector
	// usage[rid][timepoint]
	std::vector<std::vector<double>> usage;

	// TODO inner vectors should be a flat_set?
	using EventVector =
	    std::vector<boost::container::flat_set<std::pair<bool, Job::JobId>>>;
	utilities::OptionalMember<EventVector, support_iteration> events;

	std::vector<unsigned int> start_times;
};

template <bool ranged, bool single_resource>
class TreeSkyLineBase {
private:
	/*
	 * Compute value type for the tree
	 */
	constexpr static auto
	compute_value_type()
	{
		if constexpr (single_resource) {
			return hana::type_c<double>;
		} else {
			return hana::type_c<Resources>;
		}
	}

	using ValueType = typename decltype(compute_value_type())::type;

	/*
	 * Compute correct combiner type
	 */
	constexpr static auto
	compute_combiner_type()
	{
		if constexpr (!ranged) {
			return hana::type_c<ygg::MaxCombiner<unsigned int, ValueType>>;
		} else {
			return hana::type_c<ygg::RangedMaxCombiner<unsigned int, ValueType>>;
		}
	}

	using MaxCombiner = typename decltype(compute_combiner_type())::type;
	using Combiners = ygg::CombinerPack<unsigned int, ValueType, MaxCombiner>;

	class Node
	    : public ygg::DynSegTreeNodeBase<unsigned int, ValueType, ValueType,
	                                     Combiners, ygg::UseDefaultZipTree> {
	public:
		// TODO reference these? Use the direct values?
		ValueType usage;
		unsigned int start;
		unsigned int length;
	};

	class JobNodeTraits : public ygg::DynSegTreeNodeTraits<Node> {
	public:
		static unsigned int get_lower(const Node & job);
		static unsigned int get_upper(const Node & job);
		static const ValueType & get_value(const Node & job);
	};
	using Tree = ygg::DynamicSegmentTree<Node, JobNodeTraits, Combiners,
	                                     ygg::DefaultOptions, ygg::UseDefaultZipTree>;
	Tree t;

public:
	class iterator {
	private:
		using SubIterator = decltype(t.begin());

	public:
		typedef ptrdiff_t difference_type;
		typedef SkyLineEvent value_type;
		typedef const SkyLineEvent & reference;
		typedef const SkyLineEvent * pointer;
		typedef std::forward_iterator_tag iterator_category;

		iterator();
		iterator(const iterator & other);
		iterator(iterator && other);
		iterator(Tree * t, const Instance * instance, SubIterator sub_it);

		iterator & operator=(const iterator & other);
		iterator & operator=(iterator && other);

		~iterator(){};

		bool operator==(const iterator & other) const;
		bool operator!=(const iterator & other) const;

		iterator & operator++();
		iterator operator++(int);
		iterator & operator+=(size_t steps);
		iterator operator+(size_t steps) const;

		iterator & operator--();
		iterator operator--(int);
		iterator & operator-=(size_t steps);
		iterator operator-(size_t steps) const;

		reference operator*() const;
		pointer operator->() const;

		/* Conversion to generic iterator */
		// operator SkyLineIterator() noexcept;

	private:
		void update_event() noexcept;
		void update_resource_amount(bool went_forward) noexcept;

		Tree * tree;
		SubIterator sub_it;
		SkyLineEvent ev;
		const Node * ev_node;
	};

	TreeSkyLineBase(const Instance * instance);

	void remove_job(const Job & job) noexcept;
	void remove_job(Job::JobId jid) noexcept;
	void insert_job(const Job & job, unsigned int pos) noexcept;
	void insert_job(Job::JobId jid, unsigned int pos) noexcept;
	void set_pos(const Job & job, unsigned int pos) noexcept;
	void set_pos(Job::JobId jid, unsigned int pos) noexcept;

	ValueType get_maximum();
	ValueType get_maximum(unsigned int l, unsigned int r);

	using MaxRange = std::pair<unsigned int, unsigned int>;

	MaxRange get_maximum_range() const noexcept;
	MaxRange get_maximum_range(unsigned int l, unsigned int r) const noexcept;

	iterator begin();
	iterator end();

	iterator lower_bound(unsigned int x);
	iterator upper_bound(unsigned int x);

private:
	const Instance * const instance;
	std::vector<Node> nodes;
};
} // namespace ds

// The actual interface has been factored out
#include "skyline_interface.hpp"

namespace ds {
/*
 * Naming
 */
using TreeSkyLine = TreeSkyLineBase<false, false>;
using RangedTreeSkyLine = TreeSkyLineBase<true, false>;
using SingleTreeSkyLine = TreeSkyLineBase<false, true>;
using SingleRangedTreeSkyLine = TreeSkyLineBase<true, true>;

using ArraySkyLine = ArraySkyLineBase<false>;
using IteratorArraySkyLine = ArraySkyLineBase<true>;

} // namespace ds

#endif // TCPSPSUITE_SKYLINE_HPP
