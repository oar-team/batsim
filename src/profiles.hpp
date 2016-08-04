/**
 * @file profiles.hpp
 * @brief Contains profile-related structures and classes
 */

#pragma once

#include <string>
#include <map>
#include <vector>

#include <rapidjson/document.h>

/**
 * @brief Enumerates the different types of profiles
 */
enum class ProfileType
{
    DELAY                       //!< The profile is a delay. Its data is of type DelayProfileData
    ,MSG_PARALLEL               //!< The profile is composed of a computation vector and a communication matrix. Its data is of type MsgParallelProfileData
    ,MSG_PARALLEL_HOMOGENEOUS   //!< The profile is a homogeneous MSG one. Its data is of type MsgParallelHomogeneousProfileData
    ,SMPI                       //!< The profile is a SimGrid MPI time-independent trace. Its data is of type SmpiProfileData
    ,SEQUENCE                   //!< The profile is non-atomic: it is composed of a sequence of other profiles
};

/**
 * @brief Used to store profile information
 */
struct Profile
{
    /**
     * @brief Destroys a Profile, deleting its data from memory according to the profile type
     */
    ~Profile();

    ProfileType type; //!< The type of the profile
    void * data; //!< The associated data
    std::string json_description; //!< The JSON description of the profile
};

/**
 * @brief The data associated to MSG_PARALLEL profiles
 */
struct MsgParallelProfileData
{
    /**
     * @brief Destroys a MsgParallelProfileData
     * @details This method cleans the cpu and comm arrays from the memory if they are not set to nullptr
     */
    ~MsgParallelProfileData();

    int nb_res;             //!< The number of resources
    double * cpu = nullptr; //!< The computation vector
    double * com = nullptr; //!< The communication matrix
};

/**
 * @brief The data associated to MSG_PARALLEL_HOMOGENEOUS profiles
 */
struct MsgParallelHomogeneousProfileData
{
    double cpu; //!< The computation amount on each node
    double com; //!< The communication amount between each pair of nodes
};

/**
 * @brief The data associated to DELAY profiles
 */
struct DelayProfileData
{
    double delay; //!< The time amount, in seconds, that the job is supposed to take
};

/**
 * @brief The data associated to SMPI profiles
 */
struct SmpiProfileData
{
    std::vector<std::string> trace_filenames; //!< all defined tracefiles
};

/**
 * @brief The data associated to SEQUENCE profiles
 */
struct SequenceProfileData
{
    int repeat;  //!< The number of times the sequence must be repeated
    std::vector<std::string> sequence; //!< The sequence of profile names, executed in this order
};

/**
 * @brief Used to handles all the profiles of one workload
 */
class Profiles
{
public:
    /**
     * @brief Creates an empty Profiles
     */
    Profiles();
    /**
     * @brief Destroys a Profiles
     * @details All Profile elements are removed from memory
     */
    ~Profiles();

    /**
     * @brief Loads the profiles from a workload (a JSON document)
     * @param[in] doc The JSON document
     * @param[in] filename The name of the file from which the JSON document has been created (debug purpose)
     */
    void load_from_json(const rapidjson::Document & doc, const std::string & filename);

    /**
     * @brief Accesses one profile thanks to its name
     * @param[in] profile_name The name of the profile
     * @return The profile whose name is profile_name
     * @pre Such a profile exists
     */
    Profile * operator[](const std::string & profile_name);
    /**
     * @brief Accesses one profile thanks to its name (const version)
     * @param[in] profile_name The name of the profile
     * @return The profile whose name is profile_name
     * @pre Such a profile exists
     */
    const Profile * operator[](const std::string & profile_name) const;

    /**
     * @brief Accesses one profile thanks to its name
     * @param[in] profile_name The name of the profile
     * @return The profile whose name is profile_name
     * @pre Such a profile exists
     */
    Profile * at(const std::string & profile_name);
    /**
     * @brief Accesses one profile thanks to its name (const version)
     * @param[in] profile_name The name of the profile
     * @return The profile whose name is profile_name
     * @pre Such a profile exists
     */
    const Profile * at(const std::string & profile_name) const;

    /**
     * @brief Checks whether a profile exists
     * @param[in] profile_name The name of the profile
     * @return True if and only if a profile whose name is profile_name is in the Profiles
     */
    bool exists(const std::string & profile_name) const;

    /**
     * @brief Returns a copy of the internal std::map used in the Profiles
     * @return A copy of the internal std::map used in the Profiles
     */
    const std::map<std::string, Profile *> profiles() const;

private:
    std::map<std::string, Profile*> _profiles; //!< Stores all the profiles, indexed by their names
};
