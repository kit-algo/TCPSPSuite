#!/bin/bash
odb --std c++11 --database sqlite --generate-query --generate-schema --at-once \
--schema-format separate -I ../ ./db_objects/db_objects.hpp