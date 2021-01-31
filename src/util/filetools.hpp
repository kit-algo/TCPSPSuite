#ifndef TCPSPSUITE_FILETOOLS_HPP
#define TCPSPSUITE_FILETOOLS_HPP

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace util {

class LineStorage {
public:
	LineStorage(std::ifstream & f_in) : f(f_in) { this->read_lines(); }

	size_t
	line_at_byte(size_t byte) const noexcept
	{
		auto line_it = std::upper_bound(this->byte_prefix_sum.begin(),
		                                this->byte_prefix_sum.end(), byte);
		// can never be .begin() - the first entry is 0

		auto line_count = std::distance(this->byte_prefix_sum.begin(), line_it) - 1;
		assert(line_count >= 0);
		return static_cast<size_t>(line_count);
	}

	const std::string &
	get_line(size_t index) const
	{
		return this->lines.at(index);
	}

	size_t
	get_line_byte_prefix_sum(size_t index) const
	{
		return this->byte_prefix_sum.at(index);
	}

	size_t
	line_count() const noexcept
	{
		return this->lines.size();
	}

private:
	void
	read_lines()
	{
		auto old_streampos = this->f.tellg();
		this->f.seekg(0);

		for (std::string s; std::getline(this->f, s);) {
			if (not s.empty()) {
				this->byte_prefix_sum.push_back(static_cast<size_t>(this->f.tellg()));
				this->lines.push_back(s);
			}
		}

		this->f.seekg(old_streampos);
	}
	std::ifstream & f;
	std::vector<std::string> lines;
	std::vector<size_t> byte_prefix_sum;
};

class FileContextGiver {
public:
	FileContextGiver(std::string filename_in, size_t byte_in,
	                 size_t context_lines = 3)
	    : byte(byte_in), context(context_lines), filename(std::move(filename_in)),
	      f(filename), ls(f)
	{
		this->build_context();
	}

	const std::vector<std::string> &
	get_message() const noexcept
	{
		return this->message;
	}

private:
	void
	build_context()
	{
		size_t relevant_line = this->ls.line_at_byte(this->byte);
		for (size_t l = static_cast<size_t>(
		         std::max(0l, static_cast<long>(relevant_line) -
		                          static_cast<long>(this->context)));
		     l < relevant_line; ++l) {
			this->add_line(l);
		}
		this->add_line(relevant_line);
		this->add_indicator(relevant_line, byte);

		for (size_t l = relevant_line + 1;
		     l < std::min(this->ls.line_count(), relevant_line + this->context + 1);
		     ++l) {
			this->add_line(l);
		}
	}

	void
	add_line(size_t index)
	{
		std::ostringstream line;
		line << std::setfill(' ') << std::setw(4) << index << " | ";
		std::string raw_line = this->ls.get_line(index);
		std::replace(raw_line.begin(), raw_line.end(), '\t', ' ');
		line << raw_line;
		this->message.push_back(line.str());
	}

	void
	add_indicator(size_t line_index, size_t at_byte)
	{
		size_t indent = at_byte - this->ls.get_line_byte_prefix_sum(line_index);
		std::ostringstream indicator;
		std::string prefix(indent + 7, ' ');
		indicator << prefix;
		indicator << "^";
		this->message.push_back(indicator.str());
	}

	size_t byte;
	size_t context;
	std::string filename;
	std::ifstream f;

	LineStorage ls;
	std::vector<std::string> message;
};

} // namespace util

#endif
