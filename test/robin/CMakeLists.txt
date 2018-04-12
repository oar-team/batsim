enable_testing()

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
    if ("${crdf_SCHEDCMD}" STREQUAL "")
        message(FATAL_ERROR "${crdf_err_prefix} SCHEDCMD is empty")
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

function(add_test_robintest_batsched)
    # Parameters
    set(oneValueArgs NAME PLATFORM WORKLOAD
                     ALGORITHM PORT CHECK_SCRIPT
                     TEST_TIMEOUT
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

    # The following arguments have default values
    if ("${atrb_TEST_TIMEOUT}" STREQUAL "")
        set(atrb_TEST_TIMEOUT "30")
    endif()
    if ("${atrb_ALGORITHM}" STREQUAL "")
        set(atrb_ALGORITHM "fcfs_fast")
    endif()
    if ("${atrb_PORT}" STREQUAL "")
        get_property(port GLOBAL PROPERTY autoincreasing_port)
        set(atrb_PORT ${port})

        math(EXPR new_port "${port}+1" )
        set_property(GLOBAL PROPERTY autoincreasing_port "${new_port}")
    endif()
    if ("${atrb_ROBINTEST_EXPECTATION}" STREQUAL "")
        set(atrb_ROBINTEST_EXPECTATION "--expect-ctx-clean --expect-ctx-clean-at-begin --expect-ctx-clean-at-end")
    endif()

    

    # Create robin description file
    create_robin_description_file(NAME ${atrb_NAME}
        OUTPUT_DIR ${test_output_dir}/${atrb_NAME}
        BATCMD "batsim -p ${atrb_PLATFORM} -w ${atrb_WORKLOAD} -e ${test_output_dir}/${atrb_NAME}/out -s tcp://localhost:${atrb_PORT} ${atrb_OTH_BATSIM_ARGS}"
        SCHEDCMD "batsched -v ${atrb_ALGORITHM} -s tcp://*:${atrb_PORT} ${atrb_OTH_BATSCHED_ARGS}"
    )

    # Add the CMake test
    add_test(NAME ${atrb_NAME}
        COMMAND robintest ${robinfiles_dir}/${atrb_NAME}.yaml
        --test-timeout=${atrb_TEST_TIMEOUT})
endfunction()

add_test_robintest_batsched(NAME meh
    PLATFORM /home/carni/proj/batsim/platforms/small_platform.xml
    WORKLOAD /home/carni/proj/batsim/workloads/test_workload_profile.json)

add_test_robintest_batsched(NAME bwah
    PLATFORM /home/carni/proj/batsim/platforms/small_platform.xml
    WORKLOAD /home/carni/proj/batsim/workloads/test_workload_profile.json)
