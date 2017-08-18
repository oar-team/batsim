/**
 * @file machines.hpp
 * @brief Contains machine-related classes
 */

#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include <simgrid/msg.h>

#include "exact_numbers.hpp"
#include "machine_range.hpp"
#include "pstate.hpp"

struct BatsimContext;
struct Job;
class Machines;
struct MainArguments;
class PajeTracer;

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
     * @brief Constructs a Machine
     * @param[in] machines The Machines instances the Machine is within
     */
    explicit Machine(Machines * machines);

    /**
     * @brief Destroys a Machine
     */
    ~Machine();

    Machines * machines = nullptr; //!< Points to the Machines instance which contains the Machine
    int id; //!< The machine unique number
    std::string name; //!< The machine name
    msg_host_t host; //!< The SimGrid host corresponding to the machine
    MachineState state = MachineState::IDLE; //!< The current state of the Machine
    std::set<const Job *> jobs_being_computed; //!< The set of jobs being computed on the Machine

    std::map<int, PStateType> pstates; //!< Maps power state number to their power state type
    std::map<int, SleepPState *> sleep_pstates; //!< Maps sleep power state numbers to their SleepPState

    Rational last_state_change_date = 0; //!< The time at which the last state change has been done
    std::map<MachineState, Rational> time_spent_in_each_state; //!< The cumulated time of the machine in each MachineState

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

    /**
     * @brief Updates the MachineState of one machine, updating logging counters
     * @param[in] new_state The new state of the machine
     */
    void update_machine_state(MachineState new_state);
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
     * @brief Machines cannot be copied.
     * @param[in] other Another instance
     */
    Machines(const Machines & other) = delete;

    /**
     * @brief Destroys a Machines
     */
    ~Machines();

    /**
     * @brief Fill the Machines with SimGrid hosts
     * @param[in] hosts The SimGrid hosts
     * @param[in] context The Batsim Context
     * @param[in] master_host_name The name of the host which should be used as the Master host
     * @param[in] pfs_host_name The name of the host which should be used as the parallel filestem host (large-capacity storage tier)
     * @param[in] hpst_host_name The name of the host which should be used as the HPST host (high-performance storage tier)
     * @param[in] limit_machine_count If set to -1, all the machines are used. If set to a strictly positive number N, only the first machines N will be used to compute jobs
     */
    void create_machines(xbt_dynar_t hosts,
                         const BatsimContext * context,
                         const std::string & master_host_name,
                         const std::string & pfs_host_name,
                         const std::string & hpst_host_name,
                         int limit_machine_count = -1);

    /**
     * @brief Must be called when a job is executed on some machines
     * @details Puts the used machines in a computing state and traces the job beginning
     * @param[in] job The job
     * @param[in] used_machines The machines on which the job is executed
     * @param[in,out] context The Batsim Context
     */
    void update_machines_on_job_run(const Job * job,
                                    const MachineRange & used_machines,
                                    BatsimContext * context);

    /**
     * @brief Must be called when a job finishes its execution on some machines
     * @details Puts the used machines in an idle state and traces the job ending
     * @param[in] job The job
     * @param[in] used_machines The machines on which the job is executed
     * @param[in,out] context The BatsimContext
     */
    void update_machines_on_job_end(const Job *job,
                                    const MachineRange & used_machines,
                                    BatsimContext *context);

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
     * for the large-capacity storage tier.
     * @return A const pointer to the Parallel File System host machine
     */
    const Machine * pfs_machine() const;

    /**
     * @brief Returns whether or not a pfs host is registered in the system.
     * @return Whether or not a pfs host is present
     */
    bool has_pfs_machine() const;

    /**
     * @brief Returns a const pointer to the Parallel File System host machine
     * for the high-performance storage tier.
     * @return A const pointer to the Parallel File System host machine
     */
    const Machine * hpst_machine() const;

    /**
     * @brief Returns whether or not a hpst host is registered in the system.
     * @return Whether or not a hpst host is present
     */
    bool has_hpst_machine() const;

    /**
     * @brief Computes and returns the total consumed energy of all the computing machines
     * @param[in] context The Batsim context
     * @return The total consumed energy of all the computing machines
     */
    long double total_consumed_energy(const BatsimContext * context) const;

    /**
     * @brief total_wattmin Computes and returns the total wattmin (minimum power) of all the computing machines
     * @param[in] context The BatsimContext
     * @return The total wattmin (minimum power) of all the computing machines
     */
    long double total_wattmin(const BatsimContext * context) const;

    /**
     * @brief Returns the number of computing machines
     * @return The number of computing machines
     */
    int nb_machines() const;

    /**
     * @brief Updates the number of machines in each state after a MachineState transition
     * @param[in] old_state The old state of a Machine
     * @param[in] new_state The new state of a Machine
     */
    void update_nb_machines_in_each_state(MachineState old_state, MachineState new_state);

    /**
     * @brief _nb_machines_in_each_state getter
     * @return A const reference to _nb_machines_in_each_state getter
     */
    const std::map<MachineState, int> & nb_machines_in_each_state() const;

private:
    std::vector<Machine *> _machines; //!< The vector of computing machines
    Machine * _master_machine = nullptr; //!< The Master host
    Machine * _pfs_machine = nullptr; //!< The Master host
    Machine * _hpst_machine = nullptr; //!< The Master host
    PajeTracer * _tracer = nullptr; //!< The PajeTracer
    std::map<MachineState, int> _nb_machines_in_each_state; //!< Counts how many machines are in each state
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
void create_machines(const MainArguments & main_args, BatsimContext * context,
                     int max_nb_machines_to_use);

/**
 * @brief Computes the energy that has been consumed on some machines since time 0
 * @param[in] context The BatsimContext
 * @param[in] machines The machines whose energy must be computed
 * @return The energy (in joules) consumed on the machines since time 0
 */
long double consumed_energy_on_machines(BatsimContext * context,
                                        const MachineRange & machines);
