#pragma once

#include <string>
#include <map>
#include <vector>

enum class ProfileType
{
    DELAY,
    MSG_PARALLEL,
    MSG_PARALLEL_HOMOGENEOUS,
    SMPI,
    SEQUENCE
};

struct Profile
{
    ~Profile();

    ProfileType type;
    void * data;
};

struct MsgParallelProfileData
{
    ~MsgParallelProfileData();

    int nb_res;             //! The number of resources
    double * cpu = nullptr; //! The computation vector
    double * com = nullptr; //! The communication matrix
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
    ~Profiles();

    void load_from_json(const std::string & filename);

    Profile * operator[](const std::string & profile_name);
    const Profile * operator[](const std::string & profile_name) const;
    bool exists(const std::string & profile_name) const;


private:
    std::map<std::string, Profile*> _profiles;
};
