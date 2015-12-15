#pragma once

#include <string>

struct BatsimContext;

void check_worload_validity(BatsimContext * context);

void load_json_workload(BatsimContext * context, const std::string & json_filename);
