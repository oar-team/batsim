# Simulation instances
list(APPEND simu_instances "${SGPLATFORM_cluster512}?${BATWLOAD_delays}")
list(APPEND simu_instances "${SGPLATFORM_cluster512}?${BATWLOAD_long}")

# Algorithms
list(APPEND algos ${BSALGO_random})

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

        add_test_robintest_batsched(NAME "timesharing-${workload_name}-${platform_name}-${algo_name}"
            PLATFORM ${platform_filename}
            WORKLOAD ${workload_filename}
            ALGORITHM ${algo_realname}
            OTH_BATSIM_ARGS "--enable-time-sharing-on-compute"
            ROBINTEST_EXPECTATION "--expect-ctx-clean --expect-ctx-clean-at-begin --expect-ctx-clean-at-end --expect-robin-success --expect-sched-success --expect-batsim-success"
        )

        add_test_robintest_batsched(NAME "timesharing-efail-${workload_name}-${platform_name}-${algo_name}"
            PLATFORM ${platform_filename}
            WORKLOAD ${workload_filename}
            ALGORITHM ${algo_realname}
            ROBINTEST_EXPECTATION "--expect-ctx-clean --expect-ctx-clean-at-begin --expect-ctx-clean-at-end --expect-robin-failure --expect-sched-killed --expect-batsim-failure"
        )
    endforeach()
endforeach()
