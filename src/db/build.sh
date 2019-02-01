#!/bin/bash
odb --std c++11 -m dynamic --database common --database sqlite --database mysql \
    --generate-query --generate-schema --at-once --schema-format separate \
    -I ../ -I ../contrib/json ./db_objects/db_objects.hpp
