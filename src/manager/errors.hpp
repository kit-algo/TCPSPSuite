#ifndef ERRORS_H
#define ERRORS_H

#include <exception>        // for exception
#include <string>           // for string
#include <vector>           // for vector
#include "../util/log.hpp"  // for Log
class Instance;
class Storage;

class RuntimeError : public std::exception {
public:
  RuntimeError(const Instance &instance, int seed, int fault_code, std::string reason = std::string("")) noexcept;

  virtual unsigned int EXCEPTION_ID() const { return 0 ; };

  int get_seed() const;
  const Instance &get_instance() const;
  std::string get_reason() const;
  int get_fault_code() const;
  const std::vector<std::string> get_backtrace() const;

protected:
  int seed;
  int fault_code;
  const Instance &instance;
  std::string reason;
  std::vector<std::string> bt;
};

class InconsistentResultError : public RuntimeError {
public:
  InconsistentResultError(const Instance &instance, int seed, int fault_code_in, std::string reason = std::string("")) noexcept;

  virtual unsigned int EXCEPTION_ID() const { return 1 ; };
};

class InconsistentDataError : public RuntimeError {
public:
  InconsistentDataError(const Instance &instance, int seed, int fault_code_in, std::string reason = std::string("")) noexcept;

  virtual unsigned int EXCEPTION_ID() const { return 2 ; };
};

class ConfigurationError : public RuntimeError {
public:
  ConfigurationError(const Instance &instance, int seed, int fault_code_in, std::string reason = std::string("")) noexcept;

  virtual unsigned int EXCEPTION_ID() const { return 3 ; };
};

// TODO handle reason!
class ErrorHandler {
public:
  ErrorHandler(Storage &storage, std::string solver_id, std::string run_id, std::string config_id_in);
  void handle(const RuntimeError &exception);

private:
  void handle_log(const RuntimeError &exception);
  void handle_db(const RuntimeError &exception);
  void handle_fail(const RuntimeError &exception);

  Storage &storage;
  std::string solver_id;
  std::string run_id;
  std::string config_id;

  Log l;
};

#endif
