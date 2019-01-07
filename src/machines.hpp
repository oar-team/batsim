/**
 * @file machines.hpp
 * @brief Contains machine-related classes
 */

#pragma once

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <simgrid/msg.h>

#include <intervalset.hpp>

#include "pstate.hpp"
#include "permissions.hpp"

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
    simgrid::s4u::Host* host; //!< The SimGrid host corresponding to the machine
    roles::Permissions permissions = roles::Permissions::NONE; //!< Machine permissions
    MachineState state = MachineState::IDLE; //!< The current state of the Machine
    std::set<const Job *> jobs_being_computed; //!< The set of jobs being computed on the Machine

    std::unordered_map<int, PStateType> pstates; //!< Maps power state number to their power state type
    std::unordered_map<int, SleepPState *> sleep_pstates; //!< Maps sleep power state numbers to their SleepPState

    long double last_state_change_date = 0; //!< The time at which the last state change has been done
    std::unordered_map<MachineState, long double> time_spent_in_each_state; //!< The cumulated time of the machine in each MachineState

    std::unordered_map<std::string, std::string> properties; //!< Properties defined in the platform file

    /**
     * @brief Returns whether the Machine has the given role
     * @param[in] role The role whose presence is to be checked
     * @return Whether the Machine has the given role
     */
    bool has_role(roles::Permissions role) const;

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
 * @details For example, "machine2" < "machine10", "machine10_12" < "machine10_153", "machine001" == "machine1"...
 * @param s1 The first string to compare
 * @param s2 The second string to compare
 * @return Similar result than strcmp. ret<0 if s1 < s2. ret==0 if s1 == s2. Ret>0 if s1 > s2.
 */
int string_numeric_comparator(const std::string & s1, const std::string & s2);

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
     * @param[in] context The Batsim Context
     * @param[in] roles The roles of each machine
     * @param[in] limit_machine_count If set to -1, all the machines are used. If set to a strictly positive number N, only the first machines N will be used to compute jobs
     */
    void create_machines(const BatsimContext * context,
                         const std::map<std::string, std::string> & roles,
                         int limit_machine_count = -1);

    /**
     * @brief Must be called when a job is executed on some machines
     * @details Puts the used machines in a computing state and traces the job beginning
     * @param[in] job The job
     * @param[in] used_machines The machines on which the job is executed
     * @param[in,out] context The Batsim Context
     */
    void update_machines_on_job_run(const Job * job,
                                    const IntervalSet & used_machines,
                                    BatsimContext * context);

    /**
     * @brief Must be called when a job finishes its execution on some machines
     * @details Puts the used machines in an idle state and traces the job ending
     * @param[in] job The job
     * @param[in] used_machines The machines on which the job is executed
     * @param[in,out] context The BatsimContext
     */
    void update_machines_on_job_end(const Job *job,
                                    const IntervalSet & used_machines,
                                    BatsimContext *context);

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
     * @brief Returns a const reference to the vector of computing Machine
     * @return A const reference to the vector of computing Machine
     */
    const std::vector<Machine *> & compute_machines() const;

    /**
     * @brief Returns a const reference to the vector of storage Machine
     * @return A const reference to the vector of storage Machine
     */
    const std::vector<Machine *> & storage_machines() const;

    /**
     * @brief Returns a const pointer to the Master host machine
     * @return A const pointer to the Master host machine
     */
    const Machine * master_machine() const;

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
     * @brief Returns the total number of machines
     * @return The total number of machines
     */
    int nb_machines() const;

    /**
     * @brief Returns the number of computing machines
     * @return The nubmer of computing machines
     */
    int nb_compute_machines() const;

    /**
     * @brief Returns the number of storage machines
     * @return The number of storage machines
     */
    int nb_storage_machines() const;

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
    std::vector<Machine *> _machines;       //!< The vector of all machines
    std::vector<Machine *> _storage_nodes;  //!< The vector of storage machines
    std::vector<Machine *> _compute_nodes;  //!< The vector of computing machines
    Machine * _master_machine = nullptr;    //!< The master machine
    PajeTracer * _tracer = nullptr;         //!< The PajeTracer
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
                                        const IntervalSet & machines);

/**
 * @brief Sorts the given vector of machines by ascending name (lexicographically speaking)
 * @param[in,out] machines_vect The vector of machines to sort
 */
void sort_machines_by_ascending_name(std::vector<Machine *> machines_vect);


