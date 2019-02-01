#include "errors.hpp"
#include "../db/storage.hpp"        // for Storage
#include "../instance/instance.hpp" // for Instance
#include "../util/log.hpp"          // for Log
#include "../util/solverconfig.hpp"
#include <assert.h>   // for assert
#include <execinfo.h> // for backtrace
#include <stdlib.h>   // for free, size_t
#include <string>     // for string
#include <vector>     // for vector

#define BACKTRACE_SIZE 500

RuntimeError::RuntimeError(const Instance & instance_in, int seed_in,
                           int fault_code_in, std::string reason_in) noexcept
    : seed(seed_in), fault_code(fault_code_in), instance(&instance_in),
      reason(reason_in)
{
	// Get me the exception!
	void * backtrace_buffer[BACKTRACE_SIZE];
	size_t trace_size = (size_t)backtrace(backtrace_buffer, BACKTRACE_SIZE);

	char ** messages = backtrace_symbols(backtrace_buffer, (int)trace_size);

	/* skip first stack frame (points here) */
	for (size_t i = 1; i < trace_size; ++i) {
		bt.push_back(messages[i]);
	}

	free(messages);
}

InconsistentResultError::InconsistentResultError(const Instance & instance_in,
                                                 int seed_in, int fault_code_in,
                                                 std::string reason_in) noexcept
    : RuntimeError(instance_in, seed_in, fault_code_in, reason_in)
{}

InconsistentDataError::InconsistentDataError(const Instance & instance_in,
                                             int seed_in, int fault_code_in,
                                             std::string reason_in) noexcept
    : RuntimeError(instance_in, seed_in, fault_code_in, reason_in)
{}

ConfigurationError::ConfigurationError(const Instance & instance_in,
                                       int seed_in, int fault_code_in,
                                       std::string reason_in) noexcept
    : RuntimeError(instance_in, seed_in, fault_code_in, reason_in)
{}

IOError::IOError(const Instance & instance_in, int seed_in, int fault_code_in,
                 std::string reason_in) noexcept
    : RuntimeError(instance_in, seed_in, fault_code_in, reason_in)
{}

ErrorHandler::ErrorHandler(Storage & storage_in, std::string solver_id_in,
                           std::string run_id_in, std::string config_id_in,
                           const SolverConfig * sconf_in)
    : storage(storage_in), solver_id(solver_id_in), run_id(run_id_in),
      config_id(config_id_in), sconf(sconf_in), l("ERRORHANDLER")
{}

void
ErrorHandler::handle(const RuntimeError & exception)
{
	this->handle_log(exception);
	this->handle_db(exception);
#ifndef SOFT_FAIL
	this->handle_fail(exception);
#else
	BOOST_LOG(l.w()) << "Failing softly, not crashing the program.";
#endif
}

void
ErrorHandler::handle_log(const RuntimeError & exception)
{
	BOOST_LOG(l.e()) << "===========================================";
	BOOST_LOG(l.e()) << "   An error propagated into the runner.";
	BOOST_LOG(l.e()) << " Error ID:      " << exception.EXCEPTION_ID();
	BOOST_LOG(l.e()) << " Instance ID:   " << exception.get_instance().get_id();
	BOOST_LOG(l.e()) << " Instance Seed: " << exception.get_seed();
	BOOST_LOG(l.e()) << " Solver ID:     " << this->solver_id;
	BOOST_LOG(l.e()) << " Message:       " << exception.get_reason();
	BOOST_LOG(l.e()) << " Fault Code:    " << exception.get_fault_code();
	if (this->sconf != nullptr) {
		BOOST_LOG(l.e()) << "===========================================";
		BOOST_LOG(l.e()) << " Config: ";
		for (auto & [k, v] : this->sconf->get_kvs()) {
			BOOST_LOG(l.e()) << k << ":\t" << v;
		}
	}

	BOOST_LOG(l.e()) << "===========================================";
	BOOST_LOG(l.d()) << " Printing a backtrace now:";
	for (auto msg : exception.get_backtrace()) {
		BOOST_LOG(l.d()) << msg;
	}
	BOOST_LOG(l.d()) << "===========================================";
}

void
ErrorHandler::handle_fail(const RuntimeError & exception)
{
	(void)exception;
	BOOST_LOG(l.e()) << "I am configured to fail now. Have a nice day.";
	assert(false);
}

void
ErrorHandler::handle_db(const RuntimeError & exception)
{
	BOOST_LOG(l.i()) << "Logging error to storage";
	this->storage.insert_error(exception.get_instance().get_id(), this->run_id,
	                           this->solver_id, this->config_id,
	                           exception.get_seed(), exception.EXCEPTION_ID(),
	                           exception.get_fault_code());
}

std::string
RuntimeError::get_reason() const
{
	return this->reason;
}

int
RuntimeError::get_seed() const
{
	return this->seed;
}

const Instance &
RuntimeError::get_instance() const
{
	return *(this->instance);
}

int
RuntimeError::get_fault_code() const
{
	return this->fault_code;
}

const std::vector<std::string>
RuntimeError::get_backtrace() const
{
	return this->bt;
}
