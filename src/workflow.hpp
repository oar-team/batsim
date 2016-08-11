/**
 * @file workflow.hpp
 * @brief Contains workflow-related classes
 */

#pragma once

#include <string>
#include <vector>

class Job;
class Task;

/**
 * @brief A workflow is a DAG of tasks, with points to
 *        source tasks and sink tasks
 */
class Workflow
{
public:
    /**
     * @brief Builds an empty Workflow
     */
    Workflow(const std::string & name);

    /**
     * @brief Destroys a Workflow
     */
    ~Workflow();

    /**
     * @brief Loads a complete workflow from an XML filename
     * @param[in] xml_filename The name of the XML file
     */
    void load_from_xml(const std::string & xml_filename);

    /**
     * @brief Checks whether a Workflow is valid (not needed since loading from XML?)
     */
    void check_validity();


public:
    std::string name; //!< The Workflow name
    std::vector<const Task *> tasks;  //!< References to all tasks
    std::vector<const Task *> sources; //!< References to source tasks
    std::vector<const Task *> sinks; //!< References to sink tasks
};

/**
 * @brief A workflow Task is some attributes, pointers to parent tasks,
 *        and pointers to children tasks.
 */
class Task
{
public:
    /**
     * @brief Constructor
     */
    Task(const int num_procs, const double execution_time);

    /**
     * @brief Destructor
     */
    ~Task();

    /**
     * @brief Add a parent to a task
     */
    void add_parent(Task parent_task);

    /**
     * @brief Add a child to a task
     */
    void add_child(Task child_task);

    /**
     * @brief Associates a batsim Job to the task
     */
    void set_batsim_job(Job batsim_job);


public:
    int num_procs; //!< The number of processors needed for the tas
    double execution_time; //!< The execution time of the task
    Job *batsim_job; //!< The batsim job created for this task
    std::vector<const Task *> parents; //!< The parent
    std::vector<const Task *> children; //!< The children

};


