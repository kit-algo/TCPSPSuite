#ifndef TCPSPSUITE_DBMERGER_H
#define TCPSPSUITE_DBMERGER_H

#include "../db/storage.hpp" // for Storage
#include "../util/log.hpp"   // for Log
#include <memory>            // IWYU pragma: keep
#include <string>            // for string
class DBConfig;
class DBError;
class DBExtendedMeasure;
class DBIntermediate;
class DBPapiMeasurement;
class DBResourcesInfo;
class DBResult;
class DBSolution;
class DBInvocation;

class DBMerger {
public:
	DBMerger(std::string dest_filename);

	void merge(std::string src_filename);

private:
	std::shared_ptr<DBResult> copy_result(std::shared_ptr<DBResult> src);
	std::shared_ptr<DBConfig> copy_config(std::shared_ptr<DBConfig> src);
	std::shared_ptr<DBResourcesInfo>
	copy_resinfo(std::shared_ptr<DBResourcesInfo> src,
	             std::shared_ptr<DBResult> result);
	std::shared_ptr<DBPapiMeasurement>
	copy_papi_measurement(std::shared_ptr<DBPapiMeasurement> src,
	                      std::shared_ptr<DBResult> result);
	std::shared_ptr<DBSolution> copy_solution(std::shared_ptr<DBSolution> src,
	                                          std::shared_ptr<DBResult> result);
	std::shared_ptr<DBIntermediate>
	copy_intermediate(std::shared_ptr<DBIntermediate> src,
	                  std::shared_ptr<DBResult> result,
	                  std::shared_ptr<DBSolution> sol);
	std::shared_ptr<DBError> copy_error(std::shared_ptr<DBError> src);
	std::shared_ptr<DBExtendedMeasure>
	copy_extended_measure(std::shared_ptr<DBExtendedMeasure> src,
	                      std::shared_ptr<DBResult> result);

	void copy_from(Storage & src);

	std::unordered_map<unsigned long, std::shared_ptr<DBInvocation>> invocations;

	Storage dest;
	Log l;
};

#endif
