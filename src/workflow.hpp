/**
 * @file workflow.hpp
 * @brief Contains workflow-related classes
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
 * @brief A workflow is simply some Jobs with their associated Profiles
 */
class Workflow
{
public:
    /**
     * @brief Builds an empty Workflow
     */
    Workflow();

    /**
     * @brief Destroys a Workflow
     */
    ~Workflow();

    /**
     * @brief Loads a complete workflow from a XML filename
     * @param[in] xml_filename The name of the XML file
     * @param[out] nb_machines The number of machines described in the XML file
     */
    void load_from_xml(const std::string & xml_filename,
                        int & nb_machines);

    /**
     * @brief Registers SMPI applications
     */
  //    void register_smpi_applications();

    /**
     * @brief Checks whether a Workflow is valid
     */
    void check_validity();

public:
    std::string name; //!< The Workflow name
    Jobs * jobs = nullptr; //!< The Jobs of the Workflow
    Profiles * profiles = nullptr; //!< The Profiles associated to the Jobs of the Workflow
};


/**
 * @brief Handles a set of Workflows, identified by their names
 */
class Workflows
{
public:
    /**
     * @brief Builds an empty Workflows
     */
    Workflows();

    /**
     * @brief Destroys a Workflows
     */
    ~Workflows();

    /**
     * @brief Allows to access a Workflow thanks to its name
     * @param[in] workflow_name The name of the workflow to access
     * @return The workflow associated with the given workflow name
     * @pre The workflow exists
     */
    Workflow * operator[](const std::string & workflow_name);

    /**
     * @brief Allows to access a Workflow thanks to its name
     * @param[in] workflow_name The name of the workflow to access
     * @return The workflow associated with the given workflow name
     * @pre The workflow exists
     */
    const Workflow * operator[](const std::string & workflow_name) const;

    /**
     * @brief Allows to access a Workflow thanks to its name
     * @param[in] workflow_name The name of the workflow to access
     * @return The workflow associated with the given workflow name
     * @pre The workflow exists
     */
    Workflow * at(const std::string & workflow_name);

    /**
     * @brief Allows to access a Workflow thanks to its name
     * @param[in] workflow_name The name of the workflow to access
     * @return The workflow associated with the given workflow name
     * @pre The workflow exists
     */
    const Workflow * at(const std::string & workflow_name) const;

    /**
     * @brief Allows to get a job from the Workflows
     * @param[in] workflow_name The name of the workflow the job is in
     * @param[in] job_number The job number within its workflow
     * @return The job which has been asked
     * @pre The requested job exists
     */
    Job * job_at(const std::string & workflow_name, int job_number);

    /**
     * @brief Allows to get a job from the Workflows (const version)
     * @param[in] workflow_name The name of the workflow the job is in
     * @param[in] job_number The job number within its workflow
     * @return The (const) job which has been asked
     * @pre The requested job exists
     */
    const Job * job_at(const std::string & workflow_name, int job_number) const;

    /**
     * @brief Allows to get a job from the Workflows
     * @param[in] job_id The JobIdentifier
     * @return The job which has been asked
     * @pre The requested job exists
     */
    Job * job_at(const JobIdentifier & job_id);

    /**
     * @brief Allows to get a job from the Workflows (const version)
     * @param[in] job_id The JobIdentifier
     * @return The (const) job which has been asked
     * @pre The requested job exists
     */
    const Job * job_at(const JobIdentifier & job_id) const;

    /**
     * @brief Checks whether a job exist
     * @param[in] workflow_name The name of the workflow in which the job should be
     * @param[in] job_number The job number
     * @return True if the given job exists, false otherwise
     */
    bool job_exists(const std::string & workflow_name,
                    const int job_number);

    /**
     * @brief Checks whether a job exist
     * @param[in] job_id The JobIdentifier
     * @return True if the given job exists, false otherwise
     */
    bool job_exists(const JobIdentifier & job_id);

    /**
     * @brief Adds a job into memory if needed
     * @details If the jobs already exists, this method does nothing. Otherwise, the job information is loaded from the remote data storage, one Job and one Profile are created and one Workflow is created if needed.
     * @param[in] job_id The job identifier
     * @param[in,out] context The Batsim Context
     * @return The Job corresponding to job_id.
     */
    Job * add_job_if_not_exists(const JobIdentifier & job_id,
                                BatsimContext * context);

    /**
     * @brief Inserts a new Workflow into a Workflows
     * @param[in] workflow_name The name of the new Workflow to insert
     * @param[in] workflow The Workflow to insert
     * @pre There should be no existing Workflow with the same name in the Workflows
     */
    void insert_workflow(const std::string & workflow_name,
                         Workflow * workflow);

    /**
     * @brief Checks whether a Workflow with the given name exist.
     * @param[in] workflow_name The name of the Workflow whose existence is checked
     * @return true if a Workflow with the given name exists in the Workflows, false otherwise.
     */
    bool exists(const std::string & workflow_name) const;

    /**
     * @brief Gets the internal map
     * @return The internal map
     */
    std::map<std::string, Workflow*> & workflows();

    /**
     * @brief Gets the internal map (const version)
     * @return The internal map (const version)
     */
    const std::map<std::string, Workflow*> & workflows() const;

private:
    std::map<std::string, Workflow*> _workflows; //!< Associates Workflows with their names
};
