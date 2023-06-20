/**
 * @file profiles.hpp
 * @brief Contains profile-related structures and classes
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

#include <batprotocol.hpp>
#include <rapidjson/document.h>

#include "pointers.hpp"

/**
 * @brief Enumerates the different types of profiles
 */
enum class ProfileType
{
    UNSET
    ,DELAY                                     //!< a delay. Its data is of type DelayProfileData
    ,PTASK                                     //!< composed of a computation vector and a communication matrix. Its data is of type ParallelProfileData
    ,PTASK_HOMOGENEOUS                         //!< a homogeneous parallel task that executes the given amounts of computation and communication on every node. Its data is of type ParallelHomogeneousProfileData
    ,REPLAY_SMPI                               //!< a SimGrid MPI time-independent trace. Its data is of type ReplaySmpiProfileData
    ,SEQUENTIAL_COMPOSITION                    //!< non-atomic: it is composed of a sequence of other profiles
    ,PTASK_ON_STORAGE_HOMOGENEOUS              //!< Read and writes data to a PFS storage nodes. data type ParallelHomogeneousPFSProfileData
    ,PTASK_DATA_STAGING_BETWEEN_STORAGES       //!< for moving data between the pfs hosts. Its data is of type DataStagingProfileData
    ,SCHEDULER_SEND                            //!< a profile simulating a message sent to the scheduler. Its data is of type SchedulerSendProfileData
    ,SCHEDULER_RECV                            //!< receives a message from the scheduler and can execute a profile based on a value comparison of the message. Its data is of type SchedulerRecvProfileData
};

/**
 * @brief Used to store profile information
 */
struct Profile
{
    Profile() = default;

    /**
     * @brief Destroys a Profile, deleting its data from memory according to the profile type
     */
    ~Profile();

    ProfileType type; //!< The type of the profile
    void * data; //!< The associated data
    std::string json_description; //!< The JSON description of the profile
    std::string name; //!< the profile unique name
    int return_code = 0;  //!< The return code of this profile's execution (SUCCESS == 0)

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
    static ProfilePtr from_json(const std::string & profile_name,
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
    static ProfilePtr from_json(const std::string & profile_name,
                               const std::string & json_str,
                               const std::string & error_prefix = "Invalid JSON profile");

    /**
     * @brief Returns whether a profile is a parallel task (or its derivatives)
     * @return Whether a profile is a parallel task (or its derivatives)
     */
    bool is_parallel_task() const;

    /**
     * @brief Returns whether a profile is rigid or not.
     * @return Whether a profile is rigid or not.
     */
    bool is_rigid() const;
};

/**
 * @brief The data associated to PARALLEL profiles
 */
struct ParallelProfileData
{
    ParallelProfileData() = default;

    /**
     * @brief Destroys a ParallelProfileData
     * @details This method cleans the cpu and comm arrays from the memory if they are not set to nullptr
     */
    ~ParallelProfileData();

    unsigned int nb_res;    //!< The number of resources
    double * cpu = nullptr; //!< The computation vector
    double * com = nullptr; //!< The communication matrix
};

/**
 * @brief The data associated to PARALLEL_HOMOGENEOUS profiles
 */
struct ParallelHomogeneousProfileData
{
    double cpu = 0.0; //!< The computation amount on each node
    double com = 0.0; //!< The communication amount between each pair of nodes
    batprotocol::fb::HomogeneousParallelTaskGenerationStrategy strategy = batprotocol::fb::HomogeneousParallelTaskGenerationStrategy_DefinedAmountsUsedForEachValue; //!< The strategy to populate the matrices
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
struct ReplaySmpiProfileData
{
    std::vector<std::string> trace_filenames; //!< all defined tracefiles
};

/**
 * @brief The data associated to SEQUENCE profiles
 */
struct SequenceProfileData
{
    unsigned int repeat;  //!< The number of times the sequence must be repeated
    std::vector<std::string> sequence; //!< The sequence of profile names, executed in this order
    std::vector<ProfilePtr> profile_sequence; //!< The sequence of profiles, executed in this order
};

/**
 * @brief The data associated to PARALLEL_HOMOGENEOUS_PFS profiles
 */
struct ParallelTaskOnStorageHomogeneousProfileData
{
    double bytes_to_read = 0;             //!< The amount of bytes to reads from the PFS storage node for each nodes (default: 0)
    double bytes_to_write = 0;            //!< The amount of bytes to writes to the PFS storage for each nodes (default: 0)
    std::string storage_label = "pfs";    //!< A label that defines the PFS storage node (default: "pfs")
    batprotocol::fb::HomogeneousParallelTaskGenerationStrategy strategy = batprotocol::fb::HomogeneousParallelTaskGenerationStrategy_DefinedAmountsUsedForEachValue; //!< The strategy to populate the communication matrix
};

/**
 * @brief The data associated to DATA_STAGING profiles
 */
struct DataStagingProfileData
{
    double nb_bytes;                  //!< The number of bytes to transfer between the two storage nodes
    std::string from_storage_label ;  //!< The storage label where data comes from
    std::string to_storage_label ;    //!< The storage label where data goes to
};

/**
 * @brief The data associated to SCHEDULER_SEND profiles
 */
struct SchedulerSendProfileData
{
    rapidjson::Document message; //!< The message being sent to the scheduler
    double sleeptime; //!< The time to sleep after sending the message.
};

/**
 * @brief The data associated to SCHEDULER_RECV profiles
 */
struct SchedulerRecvProfileData
{
    std::string regex; //!< The regex which is tested for matching
    std::string on_success; //!< The profile to execute if it matches
    std::string on_failure; //!< The profile to execute if it does not match
    std::string on_timeout; //!< The profile to execute if no message is in the buffer (i.e. the scheduler has not answered in time). Can be omitted which will result that the job will wait until its walltime is reached.
    double polltime; //!< The time to sleep between polling if on_timeout is not set.
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
    Profiles() = default;

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
    ProfilePtr operator[](const std::string & profile_name);
    /**
     * @brief Accesses one profile thanks to its name (const version)
     * @param[in] profile_name The name of the profile
     * @return The profile whose name is profile_name
     * @pre Such a profile exists
     */
    const ProfilePtr operator[](const std::string & profile_name) const;

    /**
     * @brief Accesses one profile thanks to its name
     * @param[in] profile_name The name of the profile
     * @return The profile whose name is profile_name
     * @pre Such a profile exists
     */
    ProfilePtr at(const std::string & profile_name);
    /**
     * @brief Accesses one profile thanks to its name (const version)
     * @param[in] profile_name The name of the profile
     * @return The profile whose name is profile_name
     * @pre Such a profile exists
     */
    const ProfilePtr at(const std::string & profile_name) const;

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
    void add_profile(const std::string & profile_name, ProfilePtr & profile);

    /**
     * @brief Removes a profile from a Profiles instance (but remembers the profile existed at some point)
     * @param[in] profile_name The name of the profile to remove
     * @pre The profile exists in the Profiles instance
     */
    void remove_profile(const std::string & profile_name);

    /**
     * @brief Remove all unreferenced profiles from a Profiles instance (but remembers the profiles existed at some point)
     */
    void remove_unreferenced_profiles();

    /**
     * @brief Returns a copy of the internal std::map used in the Profiles
     * @return A copy of the internal std::map used in the Profiles
     */
    const std::unordered_map<std::string, ProfilePtr> profiles() const;

    /**
     * @brief Returns the number of profiles of the Profiles instance
     * @return The number of profiles of the Profiles instance
     */
    int nb_profiles() const;

private:
    std::unordered_map<std::string, ProfilePtr> _profiles; //!< Stores all the profiles, indexed by their names. Value can be nullptr, meaning that the profile is no longer in memory but existed in the past.
};

/**
 * @brief Returns a std::string corresponding to a given ProfileType
 * @param[in] type The ProfileType
 * @return A std::string corresponding to a given ProfileType
 */
std::string profile_type_to_string(const ProfileType & type);
