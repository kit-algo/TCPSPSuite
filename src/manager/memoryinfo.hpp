/*
 * memoryinfo.h
 *
 *  Created on: Jul 16, 2018
 *      Author: lukas
 */

#ifndef SRC_MANAGER_MEMORYINFO_HPP_
#define SRC_MANAGER_MEMORYINFO_HPP_

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <string>
#include <unordered_map>

#include "generated_config.hpp"
#include "../util/log.hpp"

namespace manager {

#ifdef PAPI_FOUND
class PAPIPerformanceInfo {
public:
	PAPIPerformanceInfo(const std::vector<std::string> & measurements) noexcept;

	void start() noexcept;
	void stop() noexcept;

	std::vector<std::pair<std::string, long long>> get_counts() const noexcept;

private:
	bool initialized;

	void initialize() noexcept;

	const std::vector<std::string> & measurements;

	size_t num_counters;

	std::vector<int> selected_events;
	std::vector<std::string> selected_event_names;
	std::vector<long long> event_counts;
	Log l;
};
#else
// Dummy s.t. we can pass null pointers around
class PAPIPerformanceInfo {};
#endif

class LinuxMemoryInfo
{
public:
	LinuxMemoryInfo(unsigned int sampling_ms);

	void start() noexcept;
	void stop() noexcept;
	void measure() noexcept;

	size_t get_rss_bytes_max() const noexcept;
	size_t get_data_bytes_max() const noexcept;

#ifdef INSTRUMENT_MALLOC
	size_t get_malloc_count() const noexcept;
	size_t get_malloc_max_size() const noexcept;
#endif

	size_t get_minor_pagefaults() const noexcept;
	size_t get_major_pagefaults() const noexcept;
	unsigned long get_user_usecs() const noexcept;
	unsigned long get_system_usecs() const noexcept;

private:
	void reset() noexcept;

	void run() noexcept;
	void sample() noexcept;

	bool stop_requested = false;

	unsigned int sampling_ms;

	size_t rss_pages_before;
	size_t rss_max_pages;
	size_t data_pages_before;
	size_t data_max_pages;
	size_t major_pagefaults_before;
	size_t major_pagefaults_after;
	size_t minor_pagefaults_before;
	size_t minor_pagefaults_after;
	unsigned long user_usecs_before;
	unsigned long user_usecs_after;
	unsigned long system_usecs_before;
	unsigned long system_usecs_after;

#ifdef INSTRUMENT_MALLOC
	size_t malloc_allocs_before;
	size_t malloc_allocs_after;
	size_t malloc_peak;
#endif

	size_t page_size;

	void get_mem_proc_data(size_t * rss_out, size_t * data_out) noexcept;
	void get_mem_syscall_data(size_t * major_pagefault_out, size_t * minor_pagefault_out,
														unsigned long * user_usecs_out, unsigned long * system_usecs_out) noexcept;

	std::thread my_thread;
	std::mutex m;
	std::condition_variable notifier;
	constexpr static int ACTION_FINISH = 0;
	constexpr static int ACTION_MEASURE = 1;
	int requested_action;
};

}

#endif /* SRC_MANAGER_MEMORYINFO_HPP_ */
