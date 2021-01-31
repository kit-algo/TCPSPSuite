/*
 * memoryinfo.cpp
 *
 *  Created on: Jul 16, 2018
 *      Author: lukas
 */

#include "memoryinfo.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
using namespace std::chrono_literals;

#ifdef PAPI_FOUND
#include <papi.h>
#endif

#ifdef INSTRUMENT_MALLOC
#include <contrib/tbingmann/malloc_tracker.hpp>
#endif

constexpr size_t BUFSIZE = 128;

namespace manager {

/**********************************************
 * Start of PAPIPerformanceInfo
 **********************************************/

#ifdef PAPI_FOUND
PAPIPerformanceInfo::PAPIPerformanceInfo(
    const std::vector<std::string> & measurements_in) noexcept
    : initialized(false), measurements(measurements_in), num_counters(0),
      l("PAPIPI")
{}

void
PAPIPerformanceInfo::initialize() noexcept
{
	this->num_counters = static_cast<size_t>(PAPI_num_counters());

	BOOST_LOG(l.d(1)) << "Your system has " << this->num_counters
	                  << " PAPI counters available.";

	int event_code;
	event_code = 0 | PAPI_NATIVE_MASK;

	this->selected_events.clear();
	this->selected_event_names.clear();
	for (const std::string & event_str : this->measurements) {
		if (PAPI_event_name_to_code(event_str.c_str(), &event_code) != PAPI_OK) {
			BOOST_LOG(l.e()) << "PAPI event " << event_str
			                 << " not found! Not measuring.";
			continue;
		}

		this->selected_event_names.push_back(event_str);
		this->selected_events.push_back(event_code);
		BOOST_LOG(l.d(3)) << "Registering PAPI event " << event_str << " (ID "
		                  << event_code << ")";
	}
	this->event_counts.resize(this->selected_events.size());

	this->initialized = true;
}

void
PAPIPerformanceInfo::start() noexcept
{
	if (!this->initialized) {
		this->initialize();
	}

	if (PAPI_start_counters(this->selected_events.data(),
	                        (int)this->selected_events.size()) != PAPI_OK) {
		BOOST_LOG(l.e()) << "Could not start PAPI measurements.";
	}
}

void
PAPIPerformanceInfo::stop() noexcept
{
	if (PAPI_stop_counters(this->event_counts.data(),
	                       (int)this->event_counts.size()) != PAPI_OK) {
		BOOST_LOG(l.e()) << "Could not stop PAPI measurements.";
	}
}

std::vector<std::pair<std::string, long long>>
PAPIPerformanceInfo::get_counts() const noexcept
{
	std::vector<std::pair<std::string, long long>> result;
	for (unsigned int i = 0; i < this->selected_events.size(); ++i) {
		result.emplace_back(this->selected_event_names[i], this->event_counts[i]);
	}

	return result;
}

#endif

/**********************************************
 * End of PAPIPerformanceInfo
 **********************************************/

LinuxMemoryInfo::LinuxMemoryInfo(unsigned int sampling_ms_in)
    : sampling_ms(sampling_ms_in),
      page_size(static_cast<size_t>(sysconf(_SC_PAGE_SIZE)))
{}

size_t
LinuxMemoryInfo::get_rss_bytes_max() const noexcept
{
	return (this->rss_max_pages - this->rss_pages_before) * this->page_size;
}

size_t
LinuxMemoryInfo::get_data_bytes_max() const noexcept
{
	return (this->data_max_pages - this->data_pages_before) * this->page_size;
}

size_t
LinuxMemoryInfo::get_minor_pagefaults() const noexcept
{
	return this->minor_pagefaults_after - this->minor_pagefaults_before;
}

size_t
LinuxMemoryInfo::get_major_pagefaults() const noexcept
{
	return this->major_pagefaults_after - this->major_pagefaults_before;
}

unsigned long
LinuxMemoryInfo::get_user_usecs() const noexcept
{
	return this->user_usecs_after - this->user_usecs_before;
}

unsigned long
LinuxMemoryInfo::get_system_usecs() const noexcept
{
	return this->system_usecs_after - this->system_usecs_before;
}

#ifdef INSTRUMENT_MALLOC
size_t
LinuxMemoryInfo::get_malloc_count() const noexcept
{
	return this->malloc_allocs_after - this->malloc_allocs_before;
}

size_t
LinuxMemoryInfo::get_malloc_max_size() const noexcept
{
	return this->malloc_peak;
}
#endif

void
LinuxMemoryInfo::reset() noexcept
{
#ifdef INSTRUMENT_MALLOC
	this->malloc_allocs_before = thrill::mem::malloc_tracker_total_allocs();
#else
	this->get_mem_proc_data(&this->rss_pages_before, &this->data_pages_before);
	this->rss_max_pages = this->rss_pages_before;
	this->data_max_pages = this->data_pages_before;
#endif

	this->get_mem_syscall_data(
	    &this->major_pagefaults_before, &this->minor_pagefaults_before,
	    &this->user_usecs_before, &this->system_usecs_before);

	this->stop_requested = false;
}

void
LinuxMemoryInfo::start() noexcept
{
	this->reset();
#ifdef INSTRUMENT_MALLOC
	thrill::mem::malloc_tracker_reset_peak();
#else
	this->my_thread = std::thread(&LinuxMemoryInfo::run, this);
#endif
}

void
LinuxMemoryInfo::run() noexcept
{
	while (true) {
		std::unique_lock<std::mutex> lock(this->m);
		this->requested_action = ACTION_MEASURE;
		this->notifier.wait_for(lock, this->sampling_ms * 1ms);

		switch (this->requested_action) {
		case ACTION_MEASURE:
			this->sample();
			break;
		case ACTION_FINISH:
			return;
			break;
		default:
			assert(false);
		}
	}
}

void
LinuxMemoryInfo::measure() noexcept
{
#ifdef INSTRUMENT_MALLOC
#else
	{
		std::lock_guard<std::mutex> guard(this->m);
		if (this->requested_action != ACTION_FINISH) {
			this->requested_action = ACTION_MEASURE;
		}
	}
	this->notifier.notify_one();
#endif
}

void
LinuxMemoryInfo::stop() noexcept
{
#ifdef INSTRUMENT_MALLOC
	this->malloc_allocs_after = thrill::mem::malloc_tracker_total_allocs();
	this->malloc_peak = thrill::mem::malloc_tracker_peak();
#else
	{
		std::lock_guard<std::mutex> guard(this->m);
		this->requested_action = ACTION_FINISH;
	}
	this->notifier.notify_one();

	this->my_thread.join();
#endif

	this->get_mem_syscall_data(
	    &this->major_pagefaults_after, &this->minor_pagefaults_after,
	    &this->user_usecs_after, &this->system_usecs_after);
}

void
LinuxMemoryInfo::sample() noexcept
{
	size_t data_pages;
	size_t rss_pages;
	this->get_mem_proc_data(&rss_pages, &data_pages);

	this->rss_max_pages = std::max(this->rss_max_pages, rss_pages);
	this->data_max_pages = std::max(this->data_max_pages, data_pages);
}

void
LinuxMemoryInfo::get_mem_syscall_data(size_t * major_pagefault_out,
                                      size_t * minor_pagefault_out,
                                      unsigned long * user_usecs_out,
                                      unsigned long * system_usecs_out) noexcept
{
	struct rusage usage;
	int result = getrusage(RUSAGE_SELF, &usage);
	if (result) {
		return;
	}

	if (major_pagefault_out != nullptr) {
		*major_pagefault_out = static_cast<size_t>(usage.ru_majflt);
	}

	if (minor_pagefault_out != nullptr) {
		*minor_pagefault_out = static_cast<size_t>(usage.ru_minflt);
	}

	if (user_usecs_out != nullptr) {
		*user_usecs_out = static_cast<size_t>((usage.ru_utime.tv_sec * 1000000) +
		                                      usage.ru_utime.tv_usec);
	}

	if (system_usecs_out != nullptr) {
		*system_usecs_out = static_cast<size_t>((usage.ru_stime.tv_sec * 1000000) +
		                                        usage.ru_stime.tv_usec);
	}
}

void
LinuxMemoryInfo::get_mem_proc_data(size_t * rss_out, size_t * data_out) noexcept
{
	char line[BUFSIZE];
	line[0] = 0;

	FILE * file = fopen("/proc/self/statm", "r");

	fgets(line, BUFSIZE, file);

	// TODO use thread-safe version here?
	// Size
	strtok(line, " ");
	// RSS
	char * rss_str = strtok(nullptr, " ");
	if (rss_out != nullptr) {
		*rss_out = static_cast<size_t>(atoi(rss_str));
	}
	// Share
	strtok(nullptr, " ");
	// Text
	strtok(nullptr, " ");
	// Lib
	strtok(nullptr, " ");
	// Data
	char * data_str = strtok(nullptr, " ");
	if (data_out != nullptr) {
		*data_out = static_cast<size_t>(atoi(data_str));
	}

	fclose(file);
}

} // namespace manager
