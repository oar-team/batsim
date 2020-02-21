/**
 * @file pointers.hpp
 * @brief Smart pointer aliases for ~garbage-collected~ data structures.
 */
#pragma once

#include <memory>

struct Job;
struct Profile;

typedef std::shared_ptr<Job> JobPtr; //!< A smart pointer towards a Job.
typedef std::shared_ptr<Profile> ProfilePtr; //!< A smart pointer towards a Profile.

typedef std::weak_ptr<Job> JobPtrWeak; //!< A weak smart pointer towards a Job (used to break reference cycles).
