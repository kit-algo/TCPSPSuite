//
// Created by lukas on 14.12.17.
//

#include "log.hpp"
#include <shared_mutex>

std::shared_timed_mutex Log::thread_id_map_mutex;
std::map<boost::log::attributes::current_thread_id::value_type, unsigned int> Log::thread_id_map;