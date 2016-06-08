/**
 * @file pstate.hpp
 * @brief Contains structures, classes and functions related to power states
 */

#pragma once

#include <map>
#include <list>
#include <string>

struct BatsimContext;

#include "machine_range.hpp"

struct Machine;

/**
 * @brief Enumerates the different types of power states
 */
enum class PStateType
{
    COMPUTATION_PSTATE          //!< Such power states can be used to compute jobs
    ,SLEEP_PSTATE               //!< Such power states cannot be used to compute jobs
    ,TRANSITION_VIRTUAL_PSTATE  //!< Such power states should only be used to transit either (from a computation power state to a sleep one) or (from a sleep power state to a computation one)
};

/**
 * @brief Stores the power states associated to one sleep power state
 */
struct SleepPState
{
    int sleep_pstate;               //!< The unique number of the sleep power state
    int switch_on_virtual_pstate;   //!< The unique number of the power state used in the transition from a computation power state to sleep_pstate
    int switch_off_virtual_pstate;  //!< The unique number of the power state used in the transition from sleep_pstate to a computation power state
};

/**
 * @brief Stores which parts of a power state switch have been done
 * @details This is done in order to acknowledge the Decision real process once all power states switches of one request have been done
 */
class CurrentSwitches
{
public:
    /**
     * @brief Stores the information related to one power state switch (possibly several machines, one target power state)
     */
    struct Switch
    {
        int target_pstate;                  //!< The power state the machines must switch to
        MachineRange all_machines;          //!< The machines considered by this state switch
        MachineRange switching_machines;    //!< The machines which are still switching to target_pstate
        std::string reply_message_content;  //!< The reply message (part of what will be acknowledged to the Decision real process) associated with this switch
    };

public:
    /**
     * @brief Adds a Switch into the CurrentSwitches
     * @param[in] machines The machines associated with the power state switch
     * @param[in] target_pstate The power states into which the machines should be put
     */
    void add_switch(const MachineRange & machines, int target_pstate);

    /**
     * @brief Marks that one machine switched its power state
     * @param[in] machine_id The unique number of the machine that just switched power state
     * @param[in] target_pstate The number of the power state into the machine just switched
     * @param[out] reply_message_content If true is returned, this argument will be set to the reply submessage associated with the switch the machine was doing
     * @param[in,out] context The Batsim context, which may be used to logging purpose
     * @return true if the machine was the last remaining one of the switch, false otherwise
     */
    bool mark_switch_as_done(int machine_id,
                             int target_pstate,
                             std::string & reply_message_content,
                             BatsimContext * context);

private:
    std::map<int, std::list<Switch *>> _switches; //!< Contains all current switches
};

/**
 * @brief Process used to switch ON a machine (transition from a sleep power state to a computation one)
 * @param[in] argc The number of arguments
 * @param[in] argv The arguments' values
 * @return 0
 */
int switch_on_machine_process(int argc, char * argv[]);

/**
 * @brief Process used to switch OFF a machine (transition from a computation power state to a sleep one)
 * @param[in] argc The number of arguments
 * @param[in] argv The arguments' values
 * @return 0
 */
int switch_off_machine_process(int argc, char * argv[]);

