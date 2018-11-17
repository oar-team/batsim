# Simulation instances
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_compute1}")
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_delay1}")
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_mixed}")
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_verylong}")
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_delay_sequences}")
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_tuto1}")
list(APPEND simu_instances "${SGPLATFORM_cluster512}?${BATWLOAD_delays}")
list(APPEND simu_instances "${SGPLATFORM_cluster512}?${BATWLOAD_long}")

# Algorithms
list(APPEND algos ${BSALGO_easy})
list(APPEND algos ${BSALGO_easy_fast})
list(APPEND algos ${BSALGO_fcfs})
list(APPEND algos ${BSALGO_filler})
list(APPEND algos ${BSALGO_rejecter})
list(APPEND algos ${BSALGO_sequencer})


foreach(simu_instance IN LISTS simu_instances)
    # Parsing simulation instance.
    string(REPLACE "?" ";" list_si ${simu_instance})
    list(GET list_si 0 platform)
    list(GET list_si 1 workload)

    string(REPLACE "!" ";" list_platform ${platform})
    list(GET list_platform 0 platform_name)
    list(GET list_platform 1 platform_filename)

    string(REPLACE "!" ";" list_workload ${workload})
    list(GET list_workload 0 workload_name)
    list(GET list_workload 1 workload_filename)

    foreach(algo IN LISTS algos)
        # Parsing algorithm.
        string(REPLACE "!" ";" list_algo ${algo})
        list(GET list_algo 0 algo_name)
        list(GET list_algo 1 algo_realname)

        # Some variables
        set(thistest_NAME "redis-${workload_name}-${platform_name}-${algo_name}")
        set(thistest_OUTPUT_DIR "${test_output_dir}/${thistest_NAME}")

        # Generate batsched input file
        file(MAKE_DIRECTORY ${thistest_OUTPUT_DIR})

        # Create the test
        add_test_robintest_batsched(NAME ${thistest_NAME}
            PLATFORM ${platform_filename}
            WORKLOAD ${workload_filename}
            ALGORITHM ${algo_realname}
            OTH_BATSIM_ARGS "--enable-redis"
            OTH_BATSCHED_ARGS ""
            ROBINTEST_EXPECTATION "--expect-ctx-clean --expect-ctx-clean-at-begin --expect-ctx-clean-at-end --expect-robin-success --expect-sched-success --expect-batsim-success"
        )
    endforeach()
endforeach()
