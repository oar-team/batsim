# Blobal variables
set_property(GLOBAL PROPERTY autoincreasing_port 28000)

# Create the directory where generated robin inputs will be stored
set(robinfiles_dir "${CMAKE_BINARY_DIR}/test-robin-files")
set(test_output_dir "${CMAKE_BINARY_DIR}/test-out")
file(MAKE_DIRECTORY ${robinfiles_dir} ${test_output_dir})

#############
# Functions #
#############
function(create_robin_description_file)
    # Parameters
    set(oneValueArgs NAME OUTPUT_DIR BATCMD SCHEDCMD
                     SIMULATION_TIMEOUT READY_TIMEOUT
                     SUCCESS_TIMEOUT FAILURE_TIMEOUT)
    cmake_parse_arguments(crdf "${options}" "${oneValueArgs}"
                               "${multiValueArgs}" ${ARGN})

    ##################
    # Default values #
    ##################
    set(crdf_err_prefix "create_robin_description_file bad call: ")

    if ("${crdf_NAME}" STREQUAL "")
        message(FATAL_ERROR "${crdf_err_prefix} NAME is empty")
    endif()
    if ("${crdf_OUTPUT_DIR}" STREQUAL "")
        message(FATAL_ERROR "${crdf_err_prefix} OUTPUT_DIR is empty")
    endif()
    if ("${crdf_BATCMD}" STREQUAL "")
        message(FATAL_ERROR "${crdf_err_prefix} BATCMD is empty")
    endif()

    if ("${crdf_SIMULATION_TIMEOUT}" STREQUAL "")
        set(crdf_SIMULATION_TIMEOUT 30)
    endif()
    if ("${crdf_READY_TIMEOUT}" STREQUAL "")
        set(crdf_READY_TIMEOUT 3)
    endif()
    if ("${crdf_SUCCESS_TIMEOUT}" STREQUAL "")
        set(crdf_SUCCESS_TIMEOUT 3)
    endif()
    if ("${crdf_FAILURE_TIMEOUT}" STREQUAL "")
        set(crdf_FAILURE_TIMEOUT 0)
    endif()

    # Call robin generate
    execute_process(COMMAND robin
        generate ${robinfiles_dir}/${crdf_NAME}.yaml
        --output-dir=${crdf_OUTPUT_DIR}
        --batcmd=${crdf_BATCMD}
        --schedcmd=${crdf_SCHEDCMD}
        --simulation-timeout=${crdf_SIMULATION_TIMEOUT}
        --ready-timeout=${crdf_READY_TIMEOUT}
        --success-timeout=${crdf_SUCCESS_TIMEOUT}
        --failure-timeout=${crdf_FAILURE_TIMEOUT}
    )
endfunction()

function(add_test_robintest)
    # Parameters
    set(oneValueArgs NAME OUTPUT_DIR BATCMD SCHEDCMD
                     SIMULATION_TIMEOUT READY_TIMEOUT
                     SUCCESS_TIMEOUT FAILURE_TIMEOUT
                     ROBINTEST_EXPECTATION TEST_TIMEOUT
                     CHECK_SCRIPT WORKING_DIR)
    cmake_parse_arguments(atr "${options}" "${oneValueArgs}"
                              "${multiValueArgs}" ${ARGN})

    ##################
    # Default values #
    ##################
    set(atr_err_prefix "add_test_robintest bad call: ")

    # The following arguments are not optional
    if ("${atr_NAME}" STREQUAL "")
        message(FATAL_ERROR "${atr_err_prefix} NAME is empty")
    endif()
    if ("${atr_OUTPUT_DIR}" STREQUAL "")
        message(FATAL_ERROR "${atr_err_prefix} OUTPUT_DIR is empty")
    endif()
    if ("${atr_BATCMD}" STREQUAL "")
        message(FATAL_ERROR "${atr_err_prefix} BATCMD is empty")
    endif()

    # The following arguments have default values
    if ("${atr_TEST_TIMEOUT}" STREQUAL "")
        set(atr_TEST_TIMEOUT "30")
    endif()
    if ("${atr_ROBINTEST_EXPECTATION}" STREQUAL "")
        set(atr_ROBINTEST_EXPECTATION "--expect-ctx-clean --expect-ctx-clean-at-begin --expect-ctx-clean-at-end")
    endif()
    if ("${atr_WORKING_DIR}" STREQUAL "")
        set(atr_WORKING_DIR "${CMAKE_SOURCE_DIR}")
    endif()

    # Create description file
    create_robin_description_file(NAME ${atr_NAME}
        OUTPUT_DIR ${atr_OUTPUT_DIR}
        BATCMD ${atr_BATCMD}
        SCHEDCMD ${atr_SCHEDCMD}
        SIMULATION_TIMEOUT ${atr_SIMULATION_TIMEOUT}
        READY_TIMEOUT ${atr_READY_TIMEOUT}
        SUCCESS_TIMEOUT ${atr_SUCCESS_TIMEOUT}
        FAILURE_TIMEOUT ${atr_FAILURE_TIMEOUT}
    )

    # Split robintest expectation by ' '
    string(REPLACE " " ";"
        ROBIN_EXPECTATION_LIST ${atr_ROBINTEST_EXPECTATION})

    # Add the CMake test
    add_test(NAME ${atr_NAME}
        COMMAND robintest ${robinfiles_dir}/${atr_NAME}.yaml
                          --test-timeout=${atr_TEST_TIMEOUT}
                          ${ROBIN_EXPECTATION_LIST}
                          --result-check-script=${atr_CHECK_SCRIPT}
        WORKING_DIRECTORY ${atr_WORKING_DIR}
    )

endfunction()

function(add_test_robintest_batsched)
    # Parameters
    set(oneValueArgs NAME PLATFORM WORKLOAD
                     ALGORITHM PORT CHECK_SCRIPT
                     TEST_TIMEOUT WORKING_DIR
                     OTH_BATSIM_ARGS OTH_BATSCHED_ARGS ROBINTEST_EXPECTATION)
    cmake_parse_arguments(atrb "${options}" "${oneValueArgs}"
                               "${multiValueArgs}" ${ARGN})

    ##################
    # Default values #
    ##################
    set(atrb_err_prefix "add_test_robintest_batsched bad call: ")

    # The following arguments are not optional
    if ("${atrb_NAME}" STREQUAL "")
        message(FATAL_ERROR "${atrb_err_prefix} NAME is empty")
    endif()
    if ("${atrb_PLATFORM}" STREQUAL "")
        message(FATAL_ERROR "${atrb_err_prefix} PLATFORM is empty")
    endif()
    if ("${atrb_WORKLOAD}" STREQUAL "")
        message(FATAL_ERROR "${atrb_err_prefix} WORKLOAD is empty")
    endif()
    if ("${atrb_ALGORITHM}" STREQUAL "")
        message(FATAL_ERROR "${atrb_err_prefix} ALGORITHM is empty")
    endif()

    # The following arguments have default values
    if ("${atrb_PORT}" STREQUAL "")
        get_property(port GLOBAL PROPERTY autoincreasing_port)
        set(atrb_PORT ${port})

        math(EXPR new_port "${port}+1" )
        set_property(GLOBAL PROPERTY autoincreasing_port "${new_port}")
    endif()

    # Create main function to add tests
    add_test_robintest(NAME ${atrb_NAME}
        OUTPUT_DIR ${test_output_dir}/${atrb_NAME}
        BATCMD "batsim -p ${atrb_PLATFORM} -w ${atrb_WORKLOAD} -e ${test_output_dir}/${atrb_NAME}/out -s tcp://localhost:${atrb_PORT} ${atrb_OTH_BATSIM_ARGS}"
        SCHEDCMD "batsched -v ${atrb_ALGORITHM} -s tcp://*:${atrb_PORT} ${atrb_OTH_BATSCHED_ARGS}"
        ROBINTEST_EXPECTATION ${atrb_ROBINTEST_EXPECTATION}
        TEST_TIMEOUT ${atrb_TEST_TIMEOUT}
        CHECK_SCRIPT ${atrb_CHECK_SCRIPT}
        WORKING_DIR ${atrb_WORKING_DIR}
    )
endfunction()


#######################################
# Definition of classical test inputs #
#######################################
# Platforms input syntax:
# "name!filename"
set(SGPLATFORM_small "small!${CMAKE_SOURCE_DIR}/platforms/small_platform.xml")
set(SGPLATFORM_cluster512 "cluster512!${CMAKE_SOURCE_DIR}/platforms/cluster512.xml")
set(SGPLATFORM_energy128 "energy128!${CMAKE_SOURCE_DIR}/platforms/energy_platform_homogeneous_no_net_128.xml")

# Workload input syntax:
# "name!filename"
set(BATWLOAD_compute1 "compute1!${CMAKE_SOURCE_DIR}/workloads/one_computation_job.json")
set(BATWLOAD_computetot1 "computetot1!${CMAKE_SOURCE_DIR}/workloads/one_computation_job_tot.json")
set(BATWLOAD_delay1 "delay1!${CMAKE_SOURCE_DIR}/workloads/one_delay_job.json")
set(BATWLOAD_delays "delays!${CMAKE_SOURCE_DIR}/workloads/simple_delay_workload.json")
set(BATWLOAD_long "long!${CMAKE_SOURCE_DIR}/workloads/batsim_paper_workload_example.json")
set(BATWLOAD_mixed "mixed!${CMAKE_SOURCE_DIR}/workloads/test_workload_profile.json")
set(BATWLOAD_verylong "verylong!${CMAKE_SOURCE_DIR}/workloads/long_workload.json")
set(BATWLOAD_walltime "walltime!${CMAKE_SOURCE_DIR}/workloads/walltime_test.json")
set(BATWLOAD_same_submit_time "samesubmittime!${CMAKE_SOURCE_DIR}/workloads/same_submit_time.json")
set(BATWLOAD_delay_sequences "delaysequences!${CMAKE_SOURCE_DIR}/workloads/test_sequence_delay.json")
set(BATWLOAD_delay_same_profiles "delayssameprofile!${CMAKE_SOURCE_DIR}/workloads/delays_same_profile.json")

set(BATWLOAD_tuto1 "tuto1!${CMAKE_SOURCE_DIR}/workloads/case_study1.json")
set(BATWLOAD_smpi_compute1 "smpicomp1!${CMAKE_SOURCE_DIR}/workloads/test_smpi_compute_only.json")
set(BATWLOAD_smpi_compute2 "smpicomp2!${CMAKE_SOURCE_DIR}/workloads/test_smpi_compute_only_2_jobs.json")
set(BATWLOAD_smpi_mixed "smpimixed!${CMAKE_SOURCE_DIR}/workloads/test_smpi_mixed_comp_comm.json")
set(BATWLOAD_smpi_mapping "smpimapping!${CMAKE_SOURCE_DIR}/workloads/test_smpi_mapping.json")

# Algorithm input syntax:
# "algo_name!algo_name_in_batsched"
set(BSALGO_easy "easy!easy_bf")
set(BSALGO_easy_fast "easyfast!easy_bf_fast")
set(BSALGO_energy_watcher "energywatcher!energy_watcher")
set(BSALGO_fcfs "fcfs!fcfs_fast")
set(BSALGO_filler "filler!filler")
set(BSALGO_random "random!random")
set(BSALGO_rejecter "rejecter!rejecter")
set(BSALGO_sequencer "sequencer!sequencer")
set(BSALGO_sleeper "sleeper!sleeper")
set(BSALGO_submitter "submitter!submitter")

##############################################
# Delegate generation of tests to subscripts #
##############################################
add_subdirectory(nosched)
add_subdirectory(simple)
add_subdirectory(energy)
add_subdirectory(energy-efail)
add_subdirectory(walltime)
add_subdirectory(timesharing)
add_subdirectory(submitorder)
add_subdirectory(redis)
add_subdirectory(dynsubmit)
add_subdirectory(kill-after10s)
add_subdirectory(kill-onnewsubmit)
add_subdirectory(kill-multiple)
add_subdirectory(kill-progress)
add_subdirectory(fewer-resources-compute1)
add_subdirectory(fewer-resources-computetot1)
add_subdirectory(set-job-metadata)
add_subdirectory(smpi-mapping)
