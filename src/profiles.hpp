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
    DELAY                                          //!< The profile is a delay. Its data is of type DelayProfileData
    ,MSG_PARALLEL                                  //!< The profile is composed of a computation vector and a communication matrix. Its data is of type MsgParallelProfileData
    ,MSG_PARALLEL_HOMOGENEOUS                      //!< The profile is a homogeneous MSG one. Its data is of type MsgParallelHomogeneousProfileData
    ,SMPI                                          //!< The profile is a SimGrid MPI time-independent trace. Its data is of type SmpiProfileData
    ,SEQUENCE                                      //!< The profile is non-atomic: it is composed of a sequence of other profiles
    ,MSG_PARALLEL_HOMOGENEOUS_PFS_MULTIPLE_TIERS   //!< The profile is a homogeneous MSG for complex parallel filesystem access. Its data is of type MsgParallelHomogeneousPFSMultipleTiersProfileData
    ,MSG_DATA_STAGING                              //!< The profile is a MSG for moving data between the pfs hosts. Its data is of type DataStagingProfileData
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

    /**
     * @brief Creates a new-allocated Profile from a JSON description
     * @param[in] profile_name The name of the profile
     * @param[in] json_desc The JSON description
     * @param[in] json_filename The JSON file name
     * @param[in] is_from_a_file Whether the JSON job comes from a file
     * @param[in] error_prefix The prefix to display when an error occurs
     * @return The new-allocated Profile
     * @pre The JSON description is valid
     */
    static Profile * from_json(const std::string & profile_name,
                               const rapidjson::Value & json_desc,
                               const std::string & error_prefix = "Invalid JSON profile",
                               bool is_from_a_file = true,
                               const std::string & json_filename = "unset");

    /**
     * @brief Creates a new-allocated Profile from a JSON description
     * @param[in] profile_name The name of the profile
     * @param[in] json_str The JSON description (as a string)
     * @param[in] error_prefix The prefix to display when an error occurs
     * @return The new-allocated Profile
     * @pre The JSON description is valid
     */
    static Profile * from_json(const std::string & profile_name,
                               const std::string & json_str,
                               const std::string & error_prefix = "Invalid JSON profile");
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
 * @brief The data associated to MSG_PARALLEL_HOMOGENEOUS_PFS_MULTIPLE_TIERS profiles
 */
struct MsgParallelHomogeneousPFSMultipleTiersProfileData
{
    enum class Direction {
        TO_STORAGE,
        FROM_STORAGE
    };

    enum class Host {
        HPST,
        LCST
    };

    double size;         //!< The size of data per compute node to transfer to pfs_machine (simulate a simple I/O traffic model)
    Direction direction; //!< Whether data should be transfered to the storage or from the storage to the nodes of the allocation
    Host host;           //!< Whether data should be transfered to/from the HPST storage or to/from the LCST storage
};

/**
 * @brief The data associated to MSG_DATA_STAGING profiles
 */
struct MsgDataStagingProfileData
{
    enum class Direction {
        LCST_TO_HPST,
        HPST_TO_LCST
    };

    double size;         //!< The size of data to transfer between the two PFS machines
    Direction direction; //!< Whether data should be transfered to the HPST or from the HPST
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
     * @brief Profiles cannot be copied.
     * @param[in] other Another instance
     */
    Profiles(const Profiles & other) = delete;

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
     * @brief Adds a Profile into a Profiles instance
     * @param[in] profile_name The name of the profile to name
     * @param[in] profile The profile to add
     * @pre No profile with the same name exists in the Profiles instance
     */
    void add_profile(const std::string & profile_name, Profile * profile);

    /**
     * @brief Returns a copy of the internal std::map used in the Profiles
     * @return A copy of the internal std::map used in the Profiles
     */
    const std::map<std::string, Profile *> profiles() const;

    /**
     * @brief Returns the number of profiles of the Profiles instance
     * @return The number of profiles of the Profiles instance
     */
    int nb_profiles() const;

private:
    std::map<std::string, Profile*> _profiles; //!< Stores all the profiles, indexed by their names
};
