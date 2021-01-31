#ifndef JSONREADER_H
#define JSONREADER_H

#include "../util/log.hpp" // for Log

#include <json.hpp>  // for json
#include <stdexcept> // for domain_error
#include <string>    // for streamsize
class Instance;

class InstanceMalformedException : public std::runtime_error {
public:
	explicit InstanceMalformedException(const char * what);
};

class JsonReader {
public:
	JsonReader(std::string filename);

	Instance * parse();

private:
	std::string filename;
	nlohmann::json js;

	Instance * instance;

	void parse_resources();
	void parse_jobs();

	template <class T>
	T
	get_json(const char * key)
	{
		try {
			return this->js[key];
		} catch (const std::domain_error & e) {
			BOOST_LOG(l.e()) << "Got an error trying to access " << key;
			throw(std::move(e));
		} catch (nlohmann::json::type_error e) {
			BOOST_LOG(l.e()) << "JSON type error:";
			BOOST_LOG(l.e()) << e.what();
			BOOST_LOG(l.e()) << "Error during access of key '" << key << "'";

			throw(std::move(e));
		}
	}

	template <class T, class json_T>
	T
	get_json(const char * key, json_T & js_in)
	{
		try {
			return js_in[key];
		} catch (const std::domain_error & e) {
			BOOST_LOG(l.e()) << "Got an error trying to access " << key;
			throw(std::move(e));
		} catch (nlohmann::json::type_error e) {
			BOOST_LOG(l.e()) << "JSON type error:";
			BOOST_LOG(l.e()) << e.what();
			BOOST_LOG(l.e()) << "Error during access of key '" << key << "'";

			throw(std::move(e));
		}
	}

	Log l;
};

#endif
