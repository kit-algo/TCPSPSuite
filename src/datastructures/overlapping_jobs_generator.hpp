#ifndef OVERLAPPING_JOBS_GENERATOR_HPP
#define OVERLAPPING_JOBS_GENERATOR_HPP

#include "../instance/instance.hpp"
#include "../instance/job.hpp"
#include "../contrib/intervaltree/src/intervaltree.hpp"
#include "../instance/laggraph.hpp"

// TODO enforce consistent windows

class OverlappingJobsGenerator {
private:
	// forward for friend declaration
	class const_iterator;
	friend class const_iterator;

	template <class Node>
	class ITreeNodeTraits : public ygg::ITreeNodeTraits<Node> {
	public:
		using key_type = unsigned int;
		static unsigned int
		get_lower(const Node & n)
		{
			return n.get_lower();
		}
		static unsigned int
		get_upper(const Node & n)
		{
			return n.get_upper();
		}
	};

	class ITreeNode
	    : public ygg::ITreeNodeBase<ITreeNode, class ITreeNodeTraits<ITreeNode>> {
	public:
		ITreeNode(const Job & job) noexcept;
		unsigned int get_lower() const noexcept;
		unsigned int get_upper() const noexcept;
		unsigned int get_jid() const noexcept;
	private:
		unsigned int jid;
		unsigned int lower;
		unsigned int upper;
	};

	const Instance & instance;

	std::vector<ITreeNode> nodes;
	std::vector<ITreeNode> dfs;
	std::vector<size_t> dfs_index;
	ygg::IntervalTree<ITreeNode, ITreeNodeTraits<ITreeNode>> itree;

	class OverlappingPair {
	public:
		unsigned int jid_a;
		unsigned int jid_b;
	};

	class const_iterator {
	public:
		typedef OverlappingPair value_type;
		typedef const OverlappingPair & const_reference;
		typedef const OverlappingPair * const_pointer;
		typedef std::input_iterator_tag
		    iterator_category; // TODO is that the right tag?

		const_iterator(const OverlappingJobsGenerator & gen);
		const_iterator(const OverlappingJobsGenerator & gen, bool);
		~const_iterator();

		// TOdO move assignment?
		// Todo move / copy construction?
		//		const_iterator & operator=(const const_iterator & other);

		bool operator==(const const_iterator & other) const noexcept;
		bool operator!=(const const_iterator & other) const noexcept;

		const_iterator & operator++() noexcept;
		const_iterator operator++(int) noexcept;

		const_reference operator*() const;
		const_pointer operator->() const;

	private:
		void advance_a() noexcept;
		void advance_b() noexcept;
		void skip_b_forward() noexcept;
		void push_a_forward() noexcept;

		OverlappingPair element;
		
		const OverlappingJobsGenerator & gen;
		size_t a_index;
		ygg::IntervalTree<ITreeNode, ITreeNodeTraits<ITreeNode>>::template QueryResult<ITreeNode> b_qr;
		decltype(b_qr.begin()) b_iterator;
		std::vector<std::vector<unsigned int>> predecessors;
	};

	
public:
	OverlappingJobsGenerator(const Instance & instance);

	const_iterator begin() const;
	const_iterator end() const;
};

#endif
