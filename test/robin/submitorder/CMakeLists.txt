# Simulation instances
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_compute1}")
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_delay1}")
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_verylong}")
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_delay_sequences}")
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_tuto1}")
list(APPEND simu_instances "${SGPLATFORM_cluster512}?${BATWLOAD_delays}")
list(APPEND simu_instances "${SGPLATFORM_cluster512}?${BATWLOAD_long}")
list(APPEND simu_instances "${SGPLATFORM_cluster512}?${BATWLOAD_same_submit_time}")

# Algorithms
list(APPEND algos ${BSALGO_easy_fast})

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

        add_test_robintest_batsched(NAME "submitorder-${workload_name}-${platform_name}-${algo_name}"
            PLATFORM ${platform_filename}
            WORKLOAD ${workload_filename}
            ALGORITHM ${algo_realname}
            ROBINTEST_EXPECTATION "--expect-ctx-clean --expect-ctx-clean-at-begin --expect-ctx-clean-at-end --expect-robin-success --expect-sched-success --expect-batsim-success"
            CHECK_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/caller.bash"
            WORKING_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
        )
    endforeach()
endforeach()
