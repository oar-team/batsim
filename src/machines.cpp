/**
 * @file machines.cpp
 * @brief Contains machine-related classes
 */

#include "machines.hpp"

#include <algorithm>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>

#include <simgrid/plugins/energy.h>

#include "batsim.hpp"
#include "context.hpp"
#include "export.hpp"
#include "jobs.hpp"
#include "permissions.hpp"

using namespace std;
using namespace roles;

XBT_LOG_NEW_DEFAULT_CATEGORY(machines, "machines"); //!< Logging

Machines::Machines()
{
    const vector<MachineState> machine_states = {MachineState::SLEEPING, MachineState::IDLE,
                                                 MachineState::COMPUTING,
                                                 MachineState::TRANSITING_FROM_SLEEPING_TO_COMPUTING,
                                                 MachineState::TRANSITING_FROM_COMPUTING_TO_SLEEPING,
                                                 MachineState::FAILED};
    for (const MachineState & state : machine_states)
    {
        _nb_machines_in_each_state[state] = 0;
    }
}

Machines::~Machines()
{
    for (Machine * machine : _machines)
    {
        delete machine;
    }
    _machines.clear();
}

void Machines::create_machines(const BatsimContext *context,
                               const std::map<string, string> & role_map,
                               int limit_machine_count)
{
    xbt_assert(_machines.size() == 0, "Bad call to Machines::createMachines(): machines already created");

    std::vector<simgrid::s4u::Host *> hosts = simgrid::s4u::Engine::get_instance()->get_all_hosts();

    for(simgrid::s4u::Host * host : hosts)
    {
        Machine * machine = new Machine(this);

        machine->name = sg_host_get_name(host);
        machine->host = host;
        machine->jobs_being_computed = {};

        machine->properties = *(host->get_properties());

        // set role properties on hosts if it has CLI defined role
        if (role_map.count(machine->name))
        {
            // there is role to set
            if (machine->properties.count("role"))
            {
                // there is an existing role property
                machine->properties["role"] = machine->properties["role"] + "," + role_map.at(machine->name);
            }
            else
            {
                machine->properties["role"] = role_map.at(machine->name);
            }
        }

        // set the permissions using the role
        machine->permissions = permissions_from_role(machine->properties["role"]);

        if (context->energy_used)
        {
            int nb_pstates = machine->host->get_pstate_count();

            auto property_it = machine->properties.find("sleep_pstates");

            // Let the sleep_pstates property be traversed in order to find the sleep and virtual transition pstates
            if (property_it != machine->properties.end())
            {
                string sleep_states_str = property_it->second;

                vector<string> sleep_pstate_triplets;
                boost::split(sleep_pstate_triplets, sleep_states_str, boost::is_any_of(","), boost::token_compress_on);

                for (const string & triplet : sleep_pstate_triplets)
                {
                    vector<string> pstates;
                    boost::split(pstates, triplet, boost::is_any_of(":"), boost::token_compress_on);
                    xbt_assert(pstates.size() == 3, "Invalid platform file '%s': host '%s' has an invalid 'sleep_pstates' property:"
                               " each comma-separated part must be composed of three pstates of three colon-separated pstates, whereas"
                               " '%s' is not valid. Each comma-separated part represents one sleep pstate sleep_ps and its virtual pstates"
                               " on_ps and off_ps used to simulate the switch ON and switch OFF mechanisms."
                               " Example of a valid comma-separated part: 0:1:3, where sleep_ps=0, on_ps=1 and off_ps=3",
                               context->platform_filename.c_str(), machine->name.c_str(), triplet.c_str());

                    int sleep_ps, on_ps, off_ps;
                    bool conversion_succeeded = true;
                    (void) conversion_succeeded; // Avoids a warning if assertions are ignored
                    try
                    {
                        boost::trim(pstates[0]);
                        boost::trim(pstates[1]);
                        boost::trim(pstates[2]);

                        sleep_ps = boost::lexical_cast<unsigned int>(pstates[0]);
                        off_ps = boost::lexical_cast<unsigned int>(pstates[1]);
                        on_ps = boost::lexical_cast<unsigned int>(pstates[2]);
                    }
                    catch(boost::bad_lexical_cast& e)
                    {
                        conversion_succeeded = false;
                    }

                    xbt_assert(conversion_succeeded, "Invalid platform file '%s': host '%s' has an invalid 'sleep_pstates' property:"
                               " the pstates of the comma-separated sleep pstate '%s' are invalid: impossible to convert the pstates to"
                               " unsigned integers", context->platform_filename.c_str(), machine->name.c_str(), triplet.c_str());

                    xbt_assert(sleep_ps >= 0 && sleep_ps < nb_pstates, "Invalid platform file '%s': host '%s' has an invalid 'sleep_pstates' property:"
                               " the pstates of the comma-separated sleep pstate '%s' are invalid: the pstate %d does not exist",
                               context->platform_filename.c_str(), machine->name.c_str(), triplet.c_str(), sleep_ps);
                    xbt_assert(on_ps >= 0 && on_ps < nb_pstates, "Invalid platform file '%s': host '%s' has an invalid 'sleep_pstates' property:"
                               " the pstates of the comma-separated sleep pstate '%s' are invalid: the pstate %d does not exist",
                               context->platform_filename.c_str(), machine->name.c_str(), triplet.c_str(), on_ps);
                    xbt_assert(off_ps >= 0 && off_ps < nb_pstates, "Invalid platform file '%s': host '%s' has an invalid 'sleep_pstates' property:"
                               " the pstates of the comma-separated sleep pstate '%s' are invalid: the sleep pstate %d does not exist",
                               context->platform_filename.c_str(), machine->name.c_str(), triplet.c_str(), off_ps);

                    if (machine->has_pstate(sleep_ps))
                    {
                        if (machine->pstates[sleep_ps] == PStateType::SLEEP_PSTATE)
                        {
                            XBT_ERROR("Invalid platform file '%s': host '%s' has an invalid 'sleep_pstates' property:"
                                      " the pstate %d is defined several times, which is forbidden.",
                                      context->platform_filename.c_str(), machine->name.c_str(), sleep_ps);
                        }
                        else if (machine->pstates[sleep_ps] == PStateType::TRANSITION_VIRTUAL_PSTATE)
                        {
                            XBT_ERROR("Invalid platform file '%s': host '%s' has an invalid 'sleep_pstates' property:"
                                      " the pstate %d is defined as a sleep pstate and as a virtual transition pstate."
                                      " A pstate can either be a computation one, a sleeping one or a virtual transition one, but combinations are forbidden.",
                                      context->platform_filename.c_str(), machine->name.c_str(), sleep_ps);
                        }
                        else
                        {
                            XBT_ERROR("Invalid platform file '%s': host '%s' has an invalid 'sleep_pstates' property:"
                                      " the pstate %d is defined as a sleep pstate and as another type of pstate."
                                      " A pstate can either be a computation one, a sleeping one or a virtual transition one, but combinations are forbidden.",
                                      context->platform_filename.c_str(), machine->name.c_str(), sleep_ps);
                        }
                    }

                    if (machine->has_pstate(on_ps))
                    {
                        xbt_assert(machine->pstates[on_ps] == PStateType::TRANSITION_VIRTUAL_PSTATE,
                                   "Invalid platform file '%s': host '%s' has an invalid 'sleep_pstates' property:"
                                   " a pstate can either be a computation one, a sleeping one or a virtual transition one, but combinations are forbidden."
                                   " Pstate %d is defined as a virtual transition pstate but also as another type of pstate.",
                                   context->platform_filename.c_str(), machine->name.c_str(), on_ps);
                    }

                    if (machine->has_pstate(off_ps))
                    {
                        xbt_assert(machine->pstates[off_ps] == PStateType::TRANSITION_VIRTUAL_PSTATE,
                                   "Invalid platform file '%s': host '%s' has an invalid 'sleep_pstates' property:"
                                   " a pstate can either be a computation one, a sleeping one or a virtual transition one, but combinations are forbidden."
                                   " Pstate %d is defined as a virtual transition pstate but also as another type of pstate.",
                                   context->platform_filename.c_str(), machine->name.c_str(), off_ps);
                    }

                    SleepPState * sleep_pstate = new SleepPState;
                    sleep_pstate->sleep_pstate = sleep_ps;
                    sleep_pstate->switch_on_virtual_pstate = on_ps;
                    sleep_pstate->switch_off_virtual_pstate = off_ps;

                    machine->sleep_pstates[sleep_ps] = sleep_pstate;
                    machine->pstates[sleep_ps] = PStateType::SLEEP_PSTATE;
                    machine->pstates[on_ps] = PStateType::TRANSITION_VIRTUAL_PSTATE;
                    machine->pstates[off_ps] = PStateType::TRANSITION_VIRTUAL_PSTATE;
                }
            }

            // Let the computation pstates be defined by those who are not sleep pstates nor virtual transition pstates
            for (int ps = 0; ps < nb_pstates; ++ps)
            {
                if (!machine->has_pstate(ps))
                {
                    // TODO: check that the pstate computational power is not null
                    machine->pstates[ps] = PStateType::COMPUTATION_PSTATE;
                }
            }
        }

        /* Guard related to the famous OBFH (https://github.com/oar-team/batsim/issues/21), which may not occur anymore.
        if (context->registration_sched_enabled)
        {
            bool contains_sleep_pstates = (machine->properties.count("sleep_pstates") == 1);
            xbt_assert(!contains_sleep_pstates,
                       "Using dynamic job submissions AND plaforms with energy information "
                       "is currently forbidden (https://github.com/oar-team/batsim/issues/21).");
        }*/

        // Machines that may compute flops must have a positive computing speed
        if ((machine->permissions & Permissions::COMPUTE_FLOPS) == Permissions::COMPUTE_FLOPS)
        {
            if (context->energy_used)
            {
                // Check all computing pstates
                for (auto mit : machine->pstates)
                {
                    int pstate_id = mit.first;
                    PStateType & pstate_type = mit.second;
                    if (pstate_type == PStateType::COMPUTATION_PSTATE)
                    {
                        xbt_assert(machine->host->get_pstate_speed(pstate_id) > 0,
                                   "Invalid platform file '%s': host '%s' has an invalid (non-positive computing speed) computing pstate %d.",
                                   context->platform_filename.c_str(), machine->name.c_str(), pstate_id);
                    }
                }
            }
            else
            {
                // Only one state to check in this case.
                xbt_assert(sg_host_speed(machine->host) > 0,
                           "Invalid platform file '%s': host '%s' is a compute node but has an invalid (non-positive) computing speed.",
                           context->platform_filename.c_str(), machine->name.c_str());
            }
        }

        // Store machines in different place depending on the role
        if (machine->permissions == Permissions::COMPUTE_NODE)
        {
            _compute_nodes.push_back(machine);
        }
        else if (machine->permissions == Permissions::STORAGE)
        {
            // wait to give ids until we know how many compute node there is
            _storage_nodes.push_back(machine);
        }
        else if (machine->permissions == Permissions::MASTER)
        {
            xbt_assert(_master_machine == nullptr, "There are two master hosts...");
            machine->id = -1;
            _master_machine = machine;
        }
    }

    // sort the machines by name
    sort_machines_by_ascending_name(_compute_nodes);

    // Let's limit the number of machines
    if (limit_machine_count != 0)
    {
        int nb_machines_without_limitation = (int)_compute_nodes.size();
        xbt_assert(limit_machine_count > 0);
        xbt_assert(limit_machine_count <= nb_machines_without_limitation,
                   "Impossible to compute on M=%d machines: "
                   "only %d machines are described in the PLATFORM_FILE.",
                   limit_machine_count,
                   nb_machines_without_limitation);

        for (int machine_id = limit_machine_count; machine_id < nb_machines_without_limitation; ++machine_id)
        {
            delete _compute_nodes[machine_id];
        }

        _compute_nodes.resize(limit_machine_count);
    }

    // Now add machines to global list and add ids
    unsigned int id = 0;
    for (auto& machine : _compute_nodes)
    {
        machine->id = id;
        ++id;
        _machines.push_back(machine);
    }

    // then attibute ids to storage machines
    for (auto& machine : _storage_nodes)
    {
        machine->id = id;
        ++id;
        _machines.push_back(machine);
    }

    xbt_assert(_master_machine != nullptr,
               "Cannot find the \"master\" role in the platform file");

    _nb_machines_in_each_state[MachineState::IDLE] = (int)_compute_nodes.size();
}

const Machine * Machines::operator[](int machineID) const
{
    xbt_assert(exists(machineID), "Cannot get machine %d: it does not exist", machineID);
    return _machines[machineID];
}

Machine * Machines::operator[](int machineID)
{
    xbt_assert(exists(machineID), "Cannot get machine %d: it does not exist", machineID);
    return _machines[machineID];
}

bool Machines::exists(int machineID) const
{
    return machineID >= 0 && machineID < (int)_machines.size();
}

void Machines::display_debug() const
{
    // Let us traverse machines to display some information about them
    vector<string> machinesVector;
    for (const Machine * m : _machines)
    {
        machinesVector.push_back(m->name + "(" + to_string(m->id) + ")");
    }

    // Let us create the string that will be sent to XBT_INFO
    string s = "Machines debug information:\n";

    s += "There are " + to_string(_machines.size()) + " machines.\n";
    s += "Mobs : [" + boost::algorithm::join(machinesVector, ", ") + "]";

    // Let us display the string which has been built
    XBT_INFO("%s", s.c_str());
}

const std::vector<Machine *> &Machines::machines() const
{
    return _machines;
}

const std::vector<Machine *> &Machines::compute_machines() const
{
    return _compute_nodes;
}

const std::vector<Machine *> &Machines::storage_machines() const
{
    return _storage_nodes;
}

const Machine *Machines::master_machine() const
{
    xbt_assert(_master_machine != nullptr,
               "Trying to access the master machine, which does not exist.");
    return _master_machine;
}

long double Machines::total_consumed_energy(const BatsimContext *context) const
{
    long double total_consumed_energy = 0;

    if (context->energy_used)
    {
        for (const Machine * m : _machines)
        {
            total_consumed_energy += sg_host_get_consumed_energy(m->host);
        }
    }
    else
    {
        total_consumed_energy = -1;
    }

    return total_consumed_energy;
}

long double Machines::total_wattmin(const BatsimContext *context) const
{
    long double total_wattmin = 0;

    if (context->energy_used)
    {
        for (const Machine * m : _machines)
        {
            total_wattmin += sg_host_get_wattmin_at(m->host, sg_host_get_pstate(m->host));
        }
    }
    else
    {
        total_wattmin = -1;
    }

    return total_wattmin;
}

int Machines::nb_machines() const
{
    return _machines.size();
}

int Machines::nb_compute_machines() const
{
    return _compute_nodes.size();
}

int Machines::nb_storage_machines() const
{
    return _storage_nodes.size();
}

void Machines::update_nb_machines_in_each_state(MachineState old_state, MachineState new_state)
{
    _nb_machines_in_each_state[old_state]--;
    _nb_machines_in_each_state[new_state]++;
}

const std::map<MachineState, int> &Machines::nb_machines_in_each_state() const
{
    return _nb_machines_in_each_state;
}

void Machines::update_machines_on_job_run(const Job * job,
                                          const IntervalSet & used_machines,
                                          BatsimContext * context)
{
    for (auto it = used_machines.elements_begin(); it != used_machines.elements_end(); ++it)
    {
        int machine_id = *it;
        Machine * machine = _machines[machine_id];
        machine->update_machine_state(MachineState::COMPUTING);

        const Job * previous_top_job = nullptr;
        if (!machine->jobs_being_computed.empty())
        {
            previous_top_job = *machine->jobs_being_computed.begin();
        }

        machine->jobs_being_computed.insert(job);

        if (previous_top_job == nullptr || previous_top_job != *machine->jobs_being_computed.begin())
        {
            if (_tracer != nullptr)
            {
                _tracer->set_machine_as_computing_job(machine->id,
                                                      *machine->jobs_being_computed.begin(),
                                                      simgrid::s4u::Engine::get_clock());
            }
        }
    }

    if (context->trace_machine_states)
    {
        context->machine_state_tracer.write_machine_states(simgrid::s4u::Engine::get_clock());
    }
}

void Machines::update_machines_on_job_end(const Job * job,
                                          const IntervalSet & used_machines,
                                          BatsimContext * context)
{
    for (auto it = used_machines.elements_begin(); it != used_machines.elements_end(); ++it)
    {
        int machine_id = *it;
        Machine * machine = _machines[machine_id];

        xbt_assert(!machine->jobs_being_computed.empty());
        const Job * previous_top_job = *machine->jobs_being_computed.begin();

        // Let's erase jobID in the jobs_being_computed data structure
        int ret = machine->jobs_being_computed.erase(job);
        (void) ret; // Avoids a warning if assertions are ignored
        xbt_assert(ret == 1);

        if (machine->jobs_being_computed.empty())
        {
            if (machine->state != MachineState::FAILED)
            {
                machine->update_machine_state(MachineState::IDLE);
                if (_tracer != nullptr)
                {
                    _tracer->set_machine_idle(machine->id, simgrid::s4u::Engine::get_clock());
                }
            }
        }
        else if (*machine->jobs_being_computed.begin() != previous_top_job)
        {
            if (_tracer != nullptr)
            {
                _tracer->set_machine_as_computing_job(machine->id,
                                                      *machine->jobs_being_computed.begin(),
                                                      simgrid::s4u::Engine::get_clock());
            }
        }
    }

    if (context->trace_machine_states)
    {
        context->machine_state_tracer.write_machine_states(simgrid::s4u::Engine::get_clock());
    }
}

void Machines::set_tracer(PajeTracer *tracer)
{
    _tracer = tracer;
}

string machine_state_to_string(MachineState state)
{
    string s;

    switch (state)
    {
    case MachineState::SLEEPING:
        s = "sleeping";
        break;
    case MachineState::IDLE:
        s = "idle";
        break;
    case MachineState::COMPUTING:
        s = "computing";
        break;
    case MachineState::TRANSITING_FROM_SLEEPING_TO_COMPUTING:
        s = "switching_on";
        break;
    case MachineState::TRANSITING_FROM_COMPUTING_TO_SLEEPING:
        s = "switching_off";
        break;
    case MachineState::FAILED:
        s = "failed";
        break;
    }

    return s;
}

Machine::Machine(Machines *machines) :
    machines(machines)
{
    xbt_assert(this->machines != nullptr);
    const vector<MachineState> machine_states = {MachineState::SLEEPING, MachineState::IDLE,
                                                 MachineState::COMPUTING,
                                                 MachineState::TRANSITING_FROM_SLEEPING_TO_COMPUTING,
                                                 MachineState::TRANSITING_FROM_COMPUTING_TO_SLEEPING,
                                                 MachineState::FAILED};
    for (const MachineState & state : machine_states)
    {
        time_spent_in_each_state[state] = 0;
    }
}

Machine::~Machine()
{
    for (auto mit : sleep_pstates)
    {
        delete mit.second;
    }

    sleep_pstates.clear();
    properties.clear();
}

bool Machine::has_role(Permissions role) const
{
    return (role & this->permissions) == role;
}

bool Machine::has_pstate(int pstate) const
{
    return pstates.find(pstate) != pstates.end();
}

void Machine::display_machine(bool is_energy_used) const
{
    // Let us traverse jobs_being_computed to display some information about them
    vector<string> jobs_vector;
    for (auto & job : jobs_being_computed)
    {
        jobs_vector.push_back(job->id.to_string());
    }

    string str = "Machine\n";
    str += "  id = " + to_string(id) + "\n";
    str += "  name = '" + name + "'\n";
    str += "  state = " + machine_state_to_string(state) + "\n";
    str += "  jobs_being_computed = [" + boost::algorithm::join(jobs_vector, ", ") + "]\n";

    if (is_energy_used)
    {
        vector<string> pstates_vector;
        vector<string> comp_pstates_vector;
        vector<string> sleep_pstates_vector;
        vector<string> vt_pstates_vector;

        for (auto & mit : pstates)
        {
            pstates_vector.push_back(to_string(mit.first));
            if (mit.second == PStateType::COMPUTATION_PSTATE)
            {
                comp_pstates_vector.push_back(to_string(mit.first));
            }
            else if (mit.second == PStateType::SLEEP_PSTATE)
            {
                sleep_pstates_vector.push_back(to_string(mit.first));
            }
            else if (mit.second == PStateType::TRANSITION_VIRTUAL_PSTATE)
            {
                vt_pstates_vector.push_back(to_string(mit.first));
            }
        }

        str += "  pstates  = [\n" + boost::algorithm::join(pstates_vector, ", ") + "]\n";
        str += "  computation pstates  = [\n" + boost::algorithm::join(comp_pstates_vector, ", ") + "]\n";
        str += "  sleep pstates  = [\n" + boost::algorithm::join(sleep_pstates_vector, ", ") + "]\n";
        str += "  virtual transition pstates  = [\n" + boost::algorithm::join(vt_pstates_vector, ", ") + "]\n";

        for (auto & mit : sleep_pstates)
        {
            str += "    sleep_ps=" + to_string(mit.second->sleep_pstate) +
                   ", on_ps=" + to_string(mit.second->switch_on_virtual_pstate) +
                   ", off_ps=" + to_string(mit.second->switch_off_virtual_pstate) + "\n";
        }
    }

    XBT_INFO("%s", str.c_str());
}

string Machine::jobs_being_computed_as_string() const
{
    vector<string> jobs_strings;

    for (auto & job : jobs_being_computed)
    {
        jobs_strings.push_back(job->id.to_string());
    }

    return boost::algorithm::join(jobs_strings, ", ");
}

void Machine::update_machine_state(MachineState new_state)
{
    long double current_date = simgrid::s4u::Engine::get_clock();

    long double delta_time = current_date - last_state_change_date;
    xbt_assert(delta_time >= 0);

    time_spent_in_each_state[state] += delta_time;

    machines->update_nb_machines_in_each_state(state, new_state);
    state = new_state;
    last_state_change_date = current_date;
}

int string_numeric_comparator(const std::string & s1, const std::string & s2)
{
    // Const C strings for s1 and s2
    const char * const_c_s1 = s1.c_str();
    const char * const_c_s2 = s2.c_str();

    // Iterators
    char * p1 = const_cast<char *>(const_c_s1);
    char * p2 = const_cast<char *>(const_c_s2);

    // End bounds (precompute for the sake of performance)
    const char * s1_end = p1 + s1.size();
    const char * s2_end = p2 + s2.size();

    // Traverse the two strings until at least one of them is at end
    for ( ; p1 < s1_end && p2 < s2_end; )
    {
        // Digits are encountered. The integer value is used as a symbol instead of the character.
        if (isdigit(*p1) && isdigit(*p2))
        {
            // The numeric value is read via strtol, which tells where it stopped its reading
            char * end_p1;
            char * end_p2;

            long int int1 = strtol(p1, &end_p1, 10);
            long int int2 = strtol(p2, &end_p2, 10);

            xbt_assert(int1 != LONG_MIN && int1 != LONG_MAX,
                       "Numeric overflow while reading %s", const_c_s1);
            xbt_assert(int2 != LONG_MIN && int1 != LONG_MAX,
                       "Numeric overflow while reading %s", const_c_s2);

            if (int1 != int2)
            {
                return int1 - int2;
            }

            // The string traversal is pursued where strtol stopped
            p1 = end_p1;
            p2 = end_p2;
        }
        else
        {
            // Classical character by character comparison
            if (*p1 != *p2)
            {
                return *p1 - *p2;
            }

            // The string traversal is pursued on the next characters
            ++p1;
            ++p2;
        }
    }

    if (p1 < s1_end)
    {
        // S1 is not at end (but S2 is since the loop condition is false)
        return 1;
    }
    else if (p2 < s2_end)
    {
        // S2 is not at end (but S1 is since the loop condition is false)
        return -1;
    }
    else
    {
        // Both strings are at end. They are equal.
        return 0;
    }
}

bool machine_comparator_name(const Machine *m1, const Machine *m2)
{
    int cmp_ret = string_numeric_comparator(m1->name, m2->name);

    // If the strings are identical in the numeric meaning, they are compared lexicographically
    if (cmp_ret == 0)
        cmp_ret = strcmp(m1->name.c_str(), m2->name.c_str());

    return cmp_ret <= 0;
}

void sort_machines_by_ascending_name(std::vector<Machine *> machines_vect)
{
    std::sort(machines_vect.begin(), machines_vect.end(), machine_comparator_name);

    for (unsigned int i = 0; i < machines_vect.size(); ++i)
    {
        machines_vect[i]->id = i;
    }
}

void create_machines(const MainArguments & main_args,
                     BatsimContext * context,
                     int max_nb_machines_to_use)
{
    XBT_INFO("Creating the machines from platform file '%s'...", main_args.platform_filename.c_str());
    simgrid::s4u::Engine::get_instance()->load_platform(main_args.platform_filename);

    XBT_INFO("Looking for master host '%s'", main_args.master_host_name.c_str());

    context->machines.create_machines(context,
                                      main_args.hosts_roles_map,
                                      max_nb_machines_to_use);

    XBT_INFO("The machines have been created successfully. There are %d computing machines.",
             context->machines.nb_compute_machines());
}

long double consumed_energy_on_machines(BatsimContext * context,
                                        const IntervalSet & machines)
{
    if (!context->energy_used)
    {
        return 0;
    }

    long double consumed_energy = 0;
    for (auto it = machines.elements_begin(); it != machines.elements_end(); ++it)
    {
        int machine_id = *it;
        Machine * machine = context->machines[machine_id];
        consumed_energy += sg_host_get_consumed_energy(machine->host);
    }

    return consumed_energy;
}
