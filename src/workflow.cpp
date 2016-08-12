/**
 * @file workflow.cpp
 * @brief Contains workflow-related functions
 */

#include "workflow.hpp"

#include <fstream>
#include <streambuf>

#include "pugixml-1.7/pugixml.hpp"

#include "context.hpp"
#include "jobs.hpp"
#include "profiles.hpp"
#include "jobs_execution.hpp"

using namespace std;
using namespace pugi;

XBT_LOG_NEW_DEFAULT_CATEGORY(workflow, "workflow"); //!< Logging

Workflow::Workflow(const std::string & name)
{
    this->name = name;
}

Workflow::~Workflow()
{
    // delete name;   TOFIX
    name = nullptr;
    tasks.clear();

}

void Workflow::load_from_xml(const std::string &xml_filename)
{
    XBT_INFO("Loading XML workflow '%s'...", xml_filename.c_str());
    // XML document creation
    xml_parse_result result = dax_tree.load_file(xml_filename.c_str());

    xbt_assert(result, "Invalid XML file");

    /*
    // Let's try to read the number of machines in the XML document
    xbt_assert(doc.HasMember("nb_res"), "Invalid XML file '%s': the 'nb_res' field is missing", xml_filename.c_str());
    const Value & nb_res_node = doc["nb_res"];
    xbt_assert(nb_res_node.IsInt(), "Invalid XML file '%s': the 'nb_res' field is not an integer", xml_filename.c_str());
    nb_machines = nb_res_node.GetInt();
    xbt_assert(nb_machines > 0, "Invalid XML file '%s': the value of the 'nb_res' field is invalid (%d)",
               xml_filename.c_str(), nb_machines);

    jobs->load_from_xml(doc, xml_filename);
    profiles->load_from_xml(doc, xml_filename);
    */
    XBT_INFO("XML workflow parsed sucessfully.");
    XBT_INFO("Checking workflow validity...");
    check_validity();
    XBT_INFO("Workflow seems to be valid.");
}


void Workflow::check_validity()
{
  // Likely not needed, so it doesn't do anything for now
  return;
}

void Workflow::add_task(Task task) {
  this->tasks.push_back(&task);
}

std::vector<Task *> Workflow::get_source_tasks() {
  std::vector<Task *> task_list;
  for(std::vector<Task *>::iterator it = this->tasks.begin(); it != this->tasks.end(); ++it) {
    if ((*it)->parents.empty()) {
      task_list.push_back(*it);
    }
  }
  return task_list;
}

std::vector<Task *> Workflow::get_sink_tasks() {
  std::vector<Task *> task_list;
  for(std::vector<Task *>::iterator it = this->tasks.begin(); it != this->tasks.end(); ++it) {
    if ((*it)->children.empty()) {
      task_list.push_back(*it);
    }
  }
  return task_list;
}



Task::Task(const int num_procs, const double execution_time)
{
    this->num_procs = num_procs;
    this->execution_time = execution_time;
    this->batsim_job = nullptr;
}

Task::~Task()
{
    parents.clear();
    children.clear();
}


void Task::add_parent(Task parent)
{
  this->parents.push_back(&parent);
}


void Task::add_child(Task child)
{
  this->children.push_back(&child);
}

void Task::set_batsim_job(Job batsim_job)
{
  this->batsim_job = &batsim_job;

}



Workflows::Workflows()
{

}

Workflows::~Workflows()
{
  for (auto mit : _workflows)
    {
      Workflow * workflow = mit.second;
      delete workflow;
    }
  _workflows.clear();
}

Workflow *Workflows::operator[](const std::string &workflow_name)
{
    return at(workflow_name);
}

const Workflow *Workflows::operator[](const std::string &workflow_name) const
{
    return at(workflow_name);
}

Workflow *Workflows::at(const std::string &workflow_name)
{
    xbt_assert(exists(workflow_name));
    //    xbt_assert(false, "The next line is bad and has been added to make Batsim compile without warning (required by Travis). Please fix it.");
    //    return nullptr;
    return _workflows.at(workflow_name);
}

const Workflow *Workflows::at(const std::string &workflow_name) const
{
    xbt_assert(exists(workflow_name));
    //xbt_assert(false, "The next line is bad and has been added to make Batsim compile without warning (required by Travis). Please fix it.");
    //return nullptr;
    return _workflows.at(workflow_name);
}

void Workflows::insert_workflow(const std::string &workflow_name, Workflow *workflow)
{
    xbt_assert(!exists(workflow_name));
    xbt_assert(!exists(workflow->name));

    workflow->name = workflow_name;
    _workflows[workflow_name] = workflow;
}

bool Workflows::exists(const std::string &workflow_name) const
{
  //xbt_assert(false, "The next line is bad and has been added to make Batsim compile without warning (required by Travis). Please fix it.");
  //return workflow_name == "mmh";
    return _workflows.count(workflow_name) == 1;
}

std::map<std::string, Workflow *> &Workflows::workflows()
{
  return _workflows;
}

const std::map<std::string, Workflow *> &Workflows::workflows() const
{
  return _workflows;
}

