#include "datastructures/overlapping_jobs_generator.hpp"

#include "algorithms/graphalgos.hpp"
#include "overlapping_jobs_generator.hpp"

#include <algorithm>

OverlappingJobsGenerator::OverlappingJobsGenerator(const Instance & instance_in)
    : instance(instance_in), dfs_index(instance.job_count(), 0)
{
	this->nodes.reserve(this->instance.job_count());
	for (const auto & job : this->instance.get_jobs()) {
		this->nodes.emplace_back(job);
	}
	for (auto & node : this->nodes) {
		this->itree.insert(node);
	}

	TopologicalSort topo(this->instance.get_laggraph());
	const auto dfs_jids = topo.get();
	size_t index = 0;
	for (const auto jid : dfs_jids) {
		this->dfs.emplace_back(this->instance.get_job(jid));
		this->dfs_index[jid] = index++;
	}
}

OverlappingJobsGenerator::const_iterator
OverlappingJobsGenerator::begin() const
{
	return const_iterator(*this);
}

OverlappingJobsGenerator::const_iterator
OverlappingJobsGenerator::end() const
{
	return const_iterator(*this, true);
}

const OverlappingJobsGenerator::OverlappingPair &
    OverlappingJobsGenerator::const_iterator::const_iterator::operator*() const
{
	return this->element;
}

OverlappingJobsGenerator::const_iterator::const_iterator(
    const OverlappingJobsGenerator & gen_in)
    : gen(gen_in), a_index(0), b_qr(gen.itree.query(gen.dfs[0])),
      b_iterator(b_qr.begin()), predecessors(gen.instance.job_count())
{
	predecessors[gen.dfs[0].get_jid()].push_back(gen.dfs[0].get_jid());

	// TODO FIXME this only works for lags == duration
	for (const auto & edge :
	     this->gen.instance.get_laggraph().neighbors(gen.dfs[0].get_jid())) {
		unsigned int t = edge.t;
		this->predecessors[t].push_back(gen.dfs[0].get_jid());
	}

	this->skip_b_forward();

	if (this->b_iterator == this->b_qr.end()) {
		this->advance_a();
	}

	if (this->b_iterator != this->b_qr.end()) {
		this->element.jid_a = this->gen.dfs[this->a_index].get_jid();
		this->element.jid_b = this->b_iterator->get_jid();
	}
}

OverlappingJobsGenerator::const_iterator::const_iterator(
    const OverlappingJobsGenerator & gen_in, bool)
    : gen(gen_in), a_index(gen.instance.job_count()),
      b_qr(gen.itree.query(gen.dfs[0])), b_iterator(b_qr.begin()),
      predecessors(gen.instance.job_count())
{}

OverlappingJobsGenerator::const_iterator::~const_iterator() {}

bool
OverlappingJobsGenerator::const_iterator::operator==(
    const const_iterator & other) const noexcept
{
	return ((this->a_index == other.a_index) &&
	        (this->b_iterator == other.b_iterator)) ||
	       (this->a_index == this->gen.instance.job_count() &&
	        other.a_index == this->gen.instance.job_count());
}

bool
OverlappingJobsGenerator::const_iterator::operator!=(
    const const_iterator & other) const noexcept
{
	return !(*this == other);
}

OverlappingJobsGenerator::const_iterator &
OverlappingJobsGenerator::const_iterator::operator++() noexcept
{
	this->advance_b();

	if (this->b_iterator == this->b_qr.end()) {
		while ((this->b_iterator == this->b_qr.end()) &&
		       (this->a_index < this->gen.instance.job_count())) {
			this->advance_a();
		}
		if (this->a_index < this->gen.instance.job_count()) {
			this->element.jid_a = this->gen.dfs[this->a_index].get_jid();
		}
	}

	if (this->b_iterator != this->b_qr.end()) {
		this->element.jid_b = this->b_iterator->get_jid();
	}

	return *this;
}

void
OverlappingJobsGenerator::const_iterator::advance_b() noexcept
{
	this->b_iterator++;
	this->skip_b_forward();
}

void
OverlappingJobsGenerator::const_iterator::push_a_forward() noexcept
{
	// Push forwards predecessors
	unsigned int jid_a = this->gen.dfs[this->a_index].get_jid();
	if (this->predecessors[jid_a].empty()) {
		this->predecessors[jid_a].push_back(jid_a);
	}

	// std::cout << "<< Pushing predecessors of " << jid_a << " forwards.\n";

	for (const auto & edge : this->gen.instance.get_laggraph().neighbors(jid_a)) {
		// std::cout << "<<< Pushing to " << edge.t << "\n";
		if (this->predecessors[edge.t].empty()) {
			this->predecessors[edge.t].push_back(edge.t);
		}

		std::vector<unsigned int> new_predecessors;

		new_predecessors.reserve(this->predecessors[jid_a].size() +
		                         this->predecessors[edge.t].size());

		std::set_union(
		    this->predecessors[jid_a].begin(), this->predecessors[jid_a].end(),
		    this->predecessors[edge.t].begin(), this->predecessors[edge.t].end(),
		    std::back_inserter(new_predecessors));
		this->predecessors[edge.t] = std::move(new_predecessors);
	}
}

void
OverlappingJobsGenerator::const_iterator::advance_a() noexcept
{
	while ((this->a_index < this->gen.instance.job_count()) &&
	       (this->b_iterator == this->b_qr.end())) {
		// Free some memory
		this->predecessors[this->gen.dfs[this->a_index].get_jid()].clear();

		this->a_index++;
		// std::cout << "++ Advancing a. A-Index now " << this->a_index << "\n";

		if (this->a_index >= this->gen.instance.job_count()) {
			return;
		}

		this->b_qr = this->gen.itree.query(this->gen.dfs[this->a_index]);
		this->b_iterator = this->b_qr.begin();

		this->push_a_forward();
		this->skip_b_forward();
	}
}

void
OverlappingJobsGenerator::const_iterator::skip_b_forward() noexcept
{
	// std::cout << "## Skipping b forwards.\n";
	const auto jid_a = this->gen.dfs[this->a_index].get_jid();

	/* We skip this b if any of:
	 * - b is part of a's predecessors
	 * - a comes before b in the topological order: if a is a predecessor of b, we
	 * cannot detect this at this stage
	 * - a == b
	 * - a.get_upper() == b.get_lower()
	 * - a.get_lower() == b.get_upper()
	 *   -> Correcting for the fact that the intervaltree does not support
	 * half-open intervals
	 */

	while ((this->b_iterator != this->b_qr.end()) &&
	       ((this->gen.dfs_index[jid_a] <
	         this->gen.dfs_index[this->b_iterator->get_jid()]) ||
	        (std::binary_search(this->predecessors[jid_a].begin(),
	                            this->predecessors[jid_a].end(),
	                            this->b_iterator->get_jid())) ||
	        (this->b_iterator->get_jid() == jid_a) ||
	        (this->b_iterator->get_lower() ==
	         this->gen.dfs[this->a_index].get_upper()) ||
	        (this->b_iterator->get_upper() ==
	         this->gen.dfs[this->a_index].get_lower()))) {

		this->b_iterator++;
	}
}

OverlappingJobsGenerator::ITreeNode::ITreeNode(const Job & job) noexcept
    : jid(job.get_jid()), lower(job.get_release()), upper(job.get_deadline())
{}

unsigned int
OverlappingJobsGenerator::ITreeNode::get_lower() const noexcept
{
	return this->lower;
}

unsigned int
OverlappingJobsGenerator::ITreeNode::get_upper() const noexcept
{
	return this->upper;
}

unsigned int
OverlappingJobsGenerator::ITreeNode::get_jid() const noexcept
{
	return this->jid;
}
