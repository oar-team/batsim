/**
 * @file workload.hpp
 * @brief Contains workload-related classes
 */

#pragma once

#include <string>
#include <map>

class Jobs;
struct Job;
class Profiles;
struct JobIdentifier;
struct BatsimContext;

/**
 * @brief A workload is simply some Jobs with their associated Profiles
 */
class Workload
{
public:
    /**
     * @brief Builds an empty Workload
     * @param[in] workload_name The workload name
     */
    Workload(const std::string & workload_name);

    /**
     * @brief Destroys a Workload
     */
    ~Workload();

    /**
     * @brief Loads a static workload from a JSON filename
     * @param[in] json_filename The name of the JSON file
     * @param[out] nb_machines The number of machines described in the JSON file
     */
    void load_from_json(const std::string & json_filename,
                        int & nb_machines);

    /**
     * @brief Registers SMPI applications
     */
    void register_smpi_applications();

    /**
     * @brief Checks whether a Workload is valid
     */
    void check_validity();

public:
    std::string name; //!< The Workload name
    Jobs * jobs = nullptr; //!< The Jobs of the Workload
    Profiles * profiles = nullptr; //!< The Profiles associated to the Jobs of the Workload
};


/**
 * @brief Handles a set of Workloads, identified by their names
 */
class Workloads
{
public:
    /**
     * @brief Builds an empty Workloads
     */
    Workloads();

    /**
     * @brief Destroys a Workloads
     */
    ~Workloads();

    /**
     * @brief Allows to access a Workload thanks to its name
     * @param[in] workload_name The name of the workload to access
     * @return The workload associated with the given workload name
     * @pre The workload exists
     */
    Workload * operator[](const std::string & workload_name);

    /**
     * @brief Allows to access a Workload thanks to its name
     * @param[in] workload_name The name of the workload to access
     * @return The workload associated with the given workload name
     * @pre The workload exists
     */
    const Workload * operator[](const std::string & workload_name) const;

    /**
     * @brief Allows to access a Workload thanks to its name
     * @param[in] workload_name The name of the workload to access
     * @return The workload associated with the given workload name
     * @pre The workload exists
     */
    Workload * at(const std::string & workload_name);

    /**
     * @brief Allows to access a Workload thanks to its name
     * @param[in] workload_name The name of the workload to access
     * @return The workload associated with the given workload name
     * @pre The workload exists
     */
    const Workload * at(const std::string & workload_name) const;

    /**
     * @brief Allows to get a job from the Workloads
     * @param[in] workload_name The name of the workload the job is in
     * @param[in] job_number The job number within its workload
     * @return The job which has been asked
     * @pre The requested job exists
     */
    Job * job_at(const std::string & workload_name, int job_number);

    /**
     * @brief Allows to get a job from the Workloads (const version)
     * @param[in] workload_name The name of the workload the job is in
     * @param[in] job_number The job number within its workload
     * @return The (const) job which has been asked
     * @pre The requested job exists
     */
    const Job * job_at(const std::string & workload_name, int job_number) const;

    /**
     * @brief Allows to get a job from the Workloads
     * @param[in] job_id The JobIdentifier
     * @return The job which has been asked
     * @pre The requested job exists
     */
    Job * job_at(const JobIdentifier & job_id);

    /**
     * @brief Allows to get a job from the Workloads (const version)
     * @param[in] job_id The JobIdentifier
     * @return The (const) job which has been asked
     * @pre The requested job exists
     */
    const Job * job_at(const JobIdentifier & job_id) const;

    /**
     * @brief Checks whether a job exist
     * @param[in] workload_name The name of the workload in which the job should be
     * @param[in] job_number The job number
     * @return True if the given job exists, false otherwise
     */
    bool job_exists(const std::string & workload_name,
                    const int job_number);

    /**
     * @brief Checks whether a job exist
     * @param[in] job_id The JobIdentifier
     * @return True if the given job exists, false otherwise
     */
    bool job_exists(const JobIdentifier & job_id);

    /**
     * @brief Adds a job into memory if needed
     * @details If the jobs already exists, this method does nothing. Otherwise, the job information is loaded from the remote data storage, one Job and one Profile are created and one Workload is created if needed.
     * @param[in] job_id The job identifier
     * @param[in,out] context The Batsim Context
     * @return The Job corresponding to job_id.
     */
    Job * add_job_if_not_exists(const JobIdentifier & job_id,
                                BatsimContext * context);

    /**
     * @brief Inserts a new Workload into a Workloads
     * @param[in] workload_name The name of the new Workload to insert
     * @param[in] workload The Workload to insert
     * @pre There should be no existing Workload with the same name in the Workloads
     */
    void insert_workload(const std::string & workload_name,
                         Workload * workload);

    /**
     * @brief Checks whether a Workload with the given name exist.
     * @param[in] workload_name The name of the Workload whose existence is checked
     * @return true if a Workload with the given name exists in the Workloads, false otherwise.
     */
    bool exists(const std::string & workload_name) const;

    /**
     * @brief Returns whether the Workload contains SMPI jobs.
     * @return true if and only if the number of SMPI jobs in the Workload is greater than 0.
     */
    bool contains_smpi_job() const;

    /**
     * @brief Registers SMPI applications
     */
    void register_smpi_applications();

    /**
     * @brief Gets the internal map
     * @return The internal map
     */
    std::map<std::string, Workload*> & workloads();

    /**
     * @brief Gets the internal map (const version)
     * @return The internal map (const version)
     */
    const std::map<std::string, Workload*> & workloads() const;

private:
    std::map<std::string, Workload*> _workloads; //!< Associates Workloads with their names
};
