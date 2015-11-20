#pragma once

#include <string>
#include <map>
#include <vector>

enum ProfileType
{
    DELAY,
    MSG_PARALLEL,
    MSG_PARALLEL_HOMOGENEOUS,
    SMPI,
    SEQUENCE
};

struct Profile
{
    ProfileType type;
    void * data;
};

struct MsgParallelProfileData
{
    int nb_res;     //! The number of resources
    double * cpu;   //! The computation vector
    double * com;   //! The communication matrix
};

struct MsgParallelHomogeneousProfileData
{
    double cpu; //! The computation amount on each node
    double com; //! The communication amount between each pair of nodes
};

struct DelayProfileData
{
    double delay; //! The time amount, in seconds, that the job is supposed to take
};

struct SmpiProfileData
{
    std::vector<std::string> trace_filenames; //! all defined tracefiles
};

//! The structure used to store additional information about profiles of type composed
struct SequenceProfileData
{
    int repeat;  //! The number of times the sequence must be repeated
    std::vector<std::string> sequence; //! The sequence of profile names
};

class Profiles
{
public:
    Profiles();

private:
    std::map<std::string, Profile> _profiles;
};
