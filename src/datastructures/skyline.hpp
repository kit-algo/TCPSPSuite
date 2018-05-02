//
// Created by lukas on 06.12.17.
//

#ifndef TCPSPSUITE_SKYLINE_HPP
#define TCPSPSUITE_SKYLINE_HPP

#include "../contrib/intervaltree/src/ygg.hpp"
#include "../instance/job.hpp"
#include "../instance/instance.hpp"

namespace ds {

template<class ValueType>
class RangedMaxCombiner {
public:
	using ValueT = ValueType;

	explicit RangedMaxCombiner(ValueT val);
	RangedMaxCombiner() = default;

	// TODO the bool is only returned for sake of expansion! Fix that!
	bool combine_with(ValueT a, ValueT edge_val);
	bool rebuild(ValueT a, ValueT a_edge_val, ValueT b, ValueT b_edge_val);

	ValueT get();

	// TODO DEBUG
	static std::string get_name() {
		return "RangedMaxCombiner";
	}
private:
	ValueT val;
};

class SkyLine
{
private:
	using MaxCombiner = ygg::MaxCombiner<double>;
	using Combiners = ygg::CombinerPack<double, MaxCombiner>;

	class Node : public ygg::DynSegTreeNodeBase<unsigned int, double, double, Combiners>
	{
	public:
		// TODO reference these? Use the direct values?
		double usage;
		unsigned int start;
		unsigned int length;
	};

private:
	class JobNodeTraits : public ygg::DynSegTreeNodeTraits<Node>
	{
	public:
		static unsigned int get_lower(const Node & job);
		static unsigned int get_upper(const Node & job);
		static double get_value(const Node & job);
	};
	using Tree = ygg::DynamicSegmentTree<Node, JobNodeTraits, Combiners>;

public:
	SkyLine(const Instance * instance, unsigned int rid);

	void set_pos(const Job & job, unsigned int pos);

	double get_maximum();

private:
	const unsigned int rid;
	const Instance * instance;
	std::vector<Node> nodes;
	Tree t;
};

} // namespace ds

#endif //TCPSPSUITE_SKYLINE_HPP
