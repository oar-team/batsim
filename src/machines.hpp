/**
 * @file machines.hpp
 * @brief Contains machine-related classes
 */

#pragma once

#include <vector>
#include <set>
#include <map>
#include <string>

#include <simgrid/msg.h>

#include "pstate.hpp"
#include "machine_range.hpp"

class PajeTracer;
struct BatsimContext;
struct MainArguments;

/**
 * @brief Enumerates the different states of a Machine
 */
enum class MachineState
{
    SLEEPING                                //!< The machine is currently sleeping
    ,IDLE                                   //!< The machine is currently idle
    ,COMPUTING                              //!< The machine is currently computing a job
    ,TRANSITING_FROM_SLEEPING_TO_COMPUTING  //!< The machine is in transition from a sleeping state to a computing state
    ,TRANSITING_FROM_COMPUTING_TO_SLEEPING  //!< The machine is in transition from a computing state to a sleeping state
};

/**
 * @brief Represents a machine
 */
struct Machine
{
    /**
     * @brief Destroys a Machine
     */
    ~Machine();

    int id; //!< The machine unique number
    std::string name; //!< The machine name
    msg_host_t host; //!< The SimGrid host corresponding to the machine
    MachineState state; //!< The current state of the Machine
    std::set<int> jobs_being_computed; //!< The set of jobs being computed on the Machine

    std::map<int, PStateType> pstates; //!< Maps power state number to their power state type
    std::map<int, SleepPState *> sleep_pstates; //!< Maps sleep power state numbers to their SleepPState

    /**
     * @brief Returns whether the Machine has the given power state
     * @param[in] pstate The power state
     * @return True if and only if the Machine has the given power state
     */
    bool has_pstate(int pstate) const;

    /**
     * @brief Displays the Machine (debug purpose)
     * @param[in] is_energy_used Must be set to true if energy information should be displayed
     */
    void display_machine(bool is_energy_used) const;

    /**
     * @brief Returns a std::string corresponding to the jobs being computed of the Machine
     * @return A std::string corresponding to the jobs being computed of the Machine
     */
    std::string jobs_being_computed_as_string() const;
};

/**
 * @brief Compares two strings. If both strings contain integers at the same place, the integers themselves are compared.
 * @details For example, "machine2" < "machine10", "machine10_12" < "machine10_153"...
 * @param s1 The first string to compare
 * @param s2 The second string to compare
 * @return True if and only if s1 < s2, taking numbers into account
 */
bool string_including_integers_comparator(const std::string & s1, const std::string & s2);

/**
 * @brief Compares two machines according to their names, taking into account numbers within them
 * @param[in] m1 The first machine
 * @param[in] m2 The second machine
 * @return True if and only if the machine name of the first machine if before the machine name of the second machine, lexicographically speaking
 */
bool machine_comparator_name(const Machine * m1, const Machine * m2);

/**
 * @brief Handles all the machines used in the simulation
 */
class Machines
{
public:
    /**
     * @brief Constructs an empty Machines
     */
    Machines();

    /**
     * @brief Destroys a Machines
     */
    ~Machines();

    /**
     * @brief Fill the Machines with SimGrid hosts
     * @param[in] hosts The SimGrid hosts
     * @param[in] context The Batsim Context
     * @param[in] masterHostName The name of the host which should be used as the Master host
     * @param[in] pfsHostName The name of the host which should be used as the parallel filestem host
     * @param[in] limit_machine_count If set to -1, all the machines are used. If set to a strictly positive number N, only the first machines N will be used to compute jobs
     */
    void create_machines(xbt_dynar_t hosts, const BatsimContext * context, const std::string & masterHostName, const std::string & pfsHostName, int limit_machine_count = -1);

    /**
     * @brief Must be called when a job is executed on some machines
     * @details Puts the used machines in a computing state and traces the job beginning
     * @param[in] jobID The unique job number
     * @param[in] usedMachines The machines on which the job is executed
     */
    void update_machines_on_job_run(int jobID, const MachineRange & usedMachines);

    /**
     * @brief Must be called when a job finishes its execution on some machines
     * @details Puts the used machines in an idle state and traces the job ending
     * @param[in] jobID The unique job number
     * @param[in] usedMachines The machines on which the job is executed
     */
    void update_machines_on_job_end(int jobID, const MachineRange & usedMachines);

    /**
     * @brief Sorts the machine by ascending name (lexicographically speaking)
     */
    void sort_machines_by_ascending_name();

    /**
     * @brief Sets the PajeTracer
     * @param[in] tracer The PajeTracer
     */
    void set_tracer(PajeTracer * tracer);

    /**
     * @brief Accesses a Machine thanks to its unique number
     * @param[in] machineID The unique machine number
     * @return The machine whose machine number is given
     */
    const Machine * operator[](int machineID) const;

    /**
     * @brief Accesses a Machine thanks to its unique number
     * @param[in] machineID The machine unique number
     * @return The machine whose machine number is given
     */
    Machine * operator[](int machineID);

    /**
     * @brief Checks whether a machine exists
     * @param[in] machineID The machine unique number
     * @return True if and only if the machine exists
     */
    bool exists(int machineID) const;

    /**
     * @brief Displays all machines (debug purpose)
     */
    void display_debug() const;

    /**
     * @brief Returns a const reference to the vector of Machine
     * @return A const reference to the vector of Machine
     */
    const std::vector<Machine *> & machines() const;

    /**
     * @brief Returns a const pointer to the Master host machine
     * @return A const pointer to the Master host machine
     */
    const Machine * master_machine() const;

    /**
     * @brief Returns a const pointer to the Parallel File System host machine
     * @return A const pointer to the Parallel File System host machine
     */
    const Machine * pfs_machine() const;

    /**
     * @brief Computes and returns the total consumed energy of all the computing machines
     * @param[in] context The Batsim context
     * @return The total consumed energy of all the computing machines
     */
    long double total_consumed_energy(const BatsimContext * context) const;

    /**
     * @brief Returns the number of computing machines
     * @return The number of computing machines
     */
    int nb_machines() const;

private:
    std::vector<Machine *> _machines; //!< The vector of computing machines
    Machine * _master_machine = nullptr; //!< The Master host
    Machine * _pfs_machine = nullptr; //!< The Master host
    PajeTracer * _tracer = nullptr; //!< The PajeTracer
};

/**
 * @brief Returns a std::string corresponding to a given MachineState
 * @param[in] state The MachineState
 * @return A std::string corresponding to a given MachineState
 */
std::string machine_state_to_string(MachineState state);

/**
 * @brief Creates the machines whose behaviour should be simulated
 * @param[in] main_args Batsim's arguments
 * @param[in] context The BatsimContext
 * @param[in] max_nb_machines_to_use The maximum number of computing machines to use
 */
void create_machines(const MainArguments & main_args, BatsimContext * context, int max_nb_machines_to_use);
