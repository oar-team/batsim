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

#include "context.hpp"
#include "export.hpp"

using namespace std;

XBT_LOG_NEW_DEFAULT_CATEGORY(machines, "machines"); //!< Logging

Machines::Machines()
{

}

Machines::~Machines()
{
    for (Machine * machine : _machines)
        delete machine;
    _machines.clear();

    if (_masterMachine != nullptr)
        delete _masterMachine;
    _masterMachine = nullptr;
}

void Machines::createMachines(xbt_dynar_t hosts, const BatsimContext *context, const string &masterHostName, int limit_machine_count)
{
    xbt_assert(_machines.size() == 0, "Bad call to Machines::createMachines(): machines already created");

    int nb_machines = xbt_dynar_length(hosts);
    _machines.reserve(nb_machines);

    msg_host_t host;
    unsigned int i, id=0;
    xbt_dynar_foreach(hosts, i, host)
    {
        Machine * machine = new Machine;

        machine->name = sg_host_get_name(host);
        machine->host = host;
        machine->jobs_being_computed = {};
        machine->state = MachineState::IDLE;

        if (context->energy_used)
        {
            int nb_pstates = MSG_host_get_nb_pstates(machine->host);
            bool contains_sleep_pstates = false;
            const char * sleep_states_cstr = MSG_host_get_property_value(machine->host, "sleep_pstates");
            contains_sleep_pstates = (sleep_states_cstr != NULL);

            // Let the sleep_pstates property be traversed in order to find the sleep and virtual transition pstates
            if (contains_sleep_pstates)
            {
                string sleep_states_str = sleep_states_cstr;

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

        if (machine->name != masterHostName)
        {
            machine->id = id;
            ++id;
            _machines.push_back(machine);
        }
        else
        {
            xbt_assert(_masterMachine == nullptr, "There are two master hosts...");
            machine->id = -1;
            _masterMachine = machine;
        }
    }

    xbt_assert(_masterMachine != nullptr, "Cannot find the MasterHost '%s' in the platform file", masterHostName.c_str());
    sortMachinesByAscendingName();

    // Let's limit the number of machines
    if (limit_machine_count != -1)
    {
        int nb_machines_without_limitation = (int)_machines.size();
        xbt_assert(limit_machine_count > 0);
        xbt_assert(limit_machine_count <= nb_machines_without_limitation,
                   "Impossible to compute on M=%d machines: "
                   "only %d machines are described in the PLATFORM_FILE.",
                   limit_machine_count,
                   nb_machines_without_limitation);

        for (int machine_id = limit_machine_count; machine_id < nb_machines_without_limitation; ++machine_id)
        {
            delete _machines[machine_id];
        }

        _machines.resize(limit_machine_count);
    }
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

void Machines::displayDebug() const
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

const Machine *Machines::masterMachine() const
{
    return _masterMachine;
}

long double Machines::total_consumed_energy(const BatsimContext *context) const
{
    long double total_consumed_energy = 0;

    if (context->energy_used)
    {
        for (const Machine * m : _machines)
            total_consumed_energy += sg_host_get_consumed_energy(m->host);
    }
    else
        total_consumed_energy = -1;

    return total_consumed_energy;
}

int Machines::nb_machines() const
{
    return _machines.size();
}

void Machines::updateMachinesOnJobRun(int jobID, const MachineRange & usedMachines)
{
    for (auto it = usedMachines.elements_begin(); it != usedMachines.elements_end(); ++it)
    {
        int machine_id = *it;
        Machine * machine = _machines[machine_id];
        machine->state = MachineState::COMPUTING;

        int previous_top_job = -1;
        if (!machine->jobs_being_computed.empty())
            previous_top_job = *machine->jobs_being_computed.begin();

        machine->jobs_being_computed.insert(jobID);

        if (previous_top_job == -1 || previous_top_job != *machine->jobs_being_computed.begin())
        {
            if (_tracer != nullptr)
                _tracer->set_machine_as_computing_job(machine->id, *machine->jobs_being_computed.begin(), MSG_get_clock());
        }
    }
}

void Machines::updateMachinesOnJobEnd(int jobID, const MachineRange & usedMachines)
{
    for (auto it = usedMachines.elements_begin(); it != usedMachines.elements_end(); ++it)
    {
        int machine_id = *it;
        Machine * machine = _machines[machine_id];

        xbt_assert(!machine->jobs_being_computed.empty());
        int previous_top_job = *machine->jobs_being_computed.begin();

        // Let's erase jobID in the jobs_being_computed data structure
        int ret = machine->jobs_being_computed.erase(jobID);
        (void) ret; // Avoids a warning if assertions are ignored
        xbt_assert(ret == 1);

        if (machine->jobs_being_computed.empty())
        {
            machine->state = MachineState::IDLE;
            if (_tracer != nullptr)
                _tracer->set_machine_idle(machine->id, MSG_get_clock());
        }
        else if (*machine->jobs_being_computed.begin() != previous_top_job)
        {
            if (_tracer != nullptr)
                _tracer->set_machine_as_computing_job(machine->id, *machine->jobs_being_computed.begin(), MSG_get_clock());
        }

    }
}

void Machines::sortMachinesByAscendingName()
{
    std::sort(_machines.begin(), _machines.end(), machine_comparator_name);

    for (unsigned int i = 0; i < _machines.size(); ++i)
        _machines[i]->id = i;
}

void Machines::setTracer(PajeTracer *tracer)
{
    _tracer = tracer;
}

string machineStateToString(MachineState state)
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
        s = "sleeping->computing";
        break;
    case MachineState::TRANSITING_FROM_COMPUTING_TO_SLEEPING:
        s = "computing->sleeping";
        break;
    }

    return s;
}

Machine::~Machine()
{
    for (auto mit : sleep_pstates)
    {
        delete mit.second;
    }

    sleep_pstates.clear();
}

bool Machine::has_pstate(int pstate) const
{
    return pstates.find(pstate) != pstates.end();
}

void Machine::display_machine(bool is_energy_used) const
{
    // Let us traverse jobs_being_computed to display some information about them
    vector<string> jobsVector;
    for (auto & jobID : jobs_being_computed)
    {
        jobsVector.push_back(std::to_string(jobID));
    }

    string str = "Machine\n";
    str += "  id = " + to_string(id) + "\n";
    str += "  name = '" + name + "'\n";
    str += "  state = " + machineStateToString(state) + "\n";
    str += "  jobs_being_computed = [" + boost::algorithm::join(jobsVector, ", ") + "]\n";

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
                comp_pstates_vector.push_back(to_string(mit.first));
            else if (mit.second == PStateType::SLEEP_PSTATE)
                sleep_pstates_vector.push_back(to_string(mit.first));
            else if (mit.second == PStateType::TRANSITION_VIRTUAL_PSTATE)
                vt_pstates_vector.push_back(to_string(mit.first));
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

    for (auto & jobID : jobs_being_computed)
        jobs_strings.push_back(to_string(jobID));

    return boost::algorithm::join(jobs_strings, ", ");
}

bool string_including_integers_comparator(const std::string & s1, const std::string & s2)
{
    int c1 = 0;
    int c2 = 0;

    int size1 = s1.length();
    int size2 = s2.length();

    for ( ; c1 < size1 && c2 < size2; )
    {
        if (isdigit(s1[c1]) && isdigit(s2[c2]))
        {
            int int1 = atoi(&s1[c1]);
            int int2 = atoi(&s2[c2]);

            if (int1 != int2)
                return int1 < int2;

            int int1_length = log10(int1) + 1;
            int int2_length = log10(int2) + 1;

            c1 += int1_length;
            c2 += int2_length;
        }
        else
        {
            if (s1[c1] != s2[c2])
                return s1[c1] < s2[c2];

            ++c1;
            ++c2;
        }
    }

    return s1.length() < s2.length();
}

bool machine_comparator_name(const Machine *m1, const Machine *m2)
{
    return string_including_integers_comparator(m1->name, m2->name);
}
