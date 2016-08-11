/**
 * @file workflow.cpp
 * @brief Contains workflow-related functions
 */

#include "workflow.hpp"

#include <fstream>
#include <streambuf>

#include <pugixml-1.7/pugixml.hpp>

#include "context.hpp"
#include "jobs.hpp"
#include "profiles.hpp"
#include "jobs_execution.hpp"

using namespace std;
// TODO : include xml namespace
//using namespace rapidjson;

XBT_LOG_NEW_DEFAULT_CATEGORY(workflow, "workflow"); //!< Logging

Workflow::Workflow(const std::string & name)
{
    this->name = workflow_name;
    this->tasks = nullptr;
    this->source_tasks = nullptr;
    this->sink_tasks = nullptr;
}

Workflow::~Workflow()
{
    delete name;
    delete tasks;
    delete sources;
    delete sinks;
}

void Workflow::load_from_xml(const std::string &xml_filename, int &nb_machines)
{
    XBT_INFO("Loading XML workflow '%s'...", xml_filename.c_str());
    // Let the file content be placed in a string
    ifstream ifile(xml_filename);
    xbt_assert(ifile.is_open(), "Cannot read file '%s'", xml_filename.c_str());
    string content;

    ifile.seekg(0, ios::end);
    content.reserve(ifile.tellg());
    ifile.seekg(0, ios::beg);

    content.assign((std::istreambuf_iterator<char>(ifile)),
                std::istreambuf_iterator<char>());

    // XML document creation
    Document doc;
    doc.Parse(content.c_str());
    xbt_assert(doc.IsObject());

    // Let's try to read the number of machines in the XML document
    xbt_assert(doc.HasMember("nb_res"), "Invalid XML file '%s': the 'nb_res' field is missing", xml_filename.c_str());
    const Value & nb_res_node = doc["nb_res"];
    xbt_assert(nb_res_node.IsInt(), "Invalid XML file '%s': the 'nb_res' field is not an integer", xml_filename.c_str());
    nb_machines = nb_res_node.GetInt();
    xbt_assert(nb_machines > 0, "Invalid XML file '%s': the value of the 'nb_res' field is invalid (%d)",
               xml_filename.c_str(), nb_machines);

    jobs->load_from_xml(doc, xml_filename);
    profiles->load_from_xml(doc, xml_filename);

    XBT_INFO("XML workflow parsed sucessfully.");
    XBT_INFO("Checking workflow validity...");
    check_validity();
    XBT_INFO("Workflow seems to be valid.");
}


void Workflow::check_validity()
{
    // Likely not needed here
    return void;
}




Task::Task(const int num_procs, const double execution_time)
{
    this->num_procs = num_procs;
    this->execution_time = execution_time;
    this->batsim_job = nullptr;
    this->parents = nullptr;
    this->children = nullptr;
}

Task::~Task()
{
    delete batsim_job
    delete parents;
    delete children;
}


