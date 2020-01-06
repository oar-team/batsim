/**
 * @file pointers.hpp
 * @brief Smart pointer aliases for ~garbage-collected~ data structures.
 */
#pragma once

#include <memory>

struct Job;
struct Profile;

typedef std::shared_ptr<Job> JobPtr;
typedef std::shared_ptr<Profile> ProfilePtr;
