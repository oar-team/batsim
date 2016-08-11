/**
 * @file workflow.hpp
 * @brief Contains workflow-related classes
 */

#pragma once

#include <string>
#include <vector>

#ifndef HEADER_PUGIXML_HPP
#       include "pugixml-1.7/pugixml.hpp"
#endif

struct Job;
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

    /**
     * @brief Adds a task to the workflow
     */
    void add_task(Task task);

    /**
     * @brief Get source tasks
     */
    std::vector<Task>* get_source_tasks();

    /**
     * @brief Get sink tasks
     */
    std::vector<Task>* get_sink_tasks();

public:
    std::string name; //!< The Workflow name
    std::vector<Task> tasks;  //!< References to all tasks

private:
    pugi::xml_document dax_tree; //!< The DAX tree
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
    std::vector<Task> parents; //!< The parent
    std::vector<Task> children; //!< The children

};


