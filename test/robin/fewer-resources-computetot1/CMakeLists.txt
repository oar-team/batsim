# Simulation instances
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_computetot1}")

# Algorithms
list(APPEND algos ${BSALGO_filler})

# Fraction of resources to use
list(APPEND fractions_of_resources_to_use "0.25!0.25")
list(APPEND fractions_of_resources_to_use "0.50!0.50")
list(APPEND fractions_of_resources_to_use "0.75!0.75")
list(APPEND fractions_of_resources_to_use "1.00!1.00")

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

        foreach(frtu IN LISTS fractions_of_resources_to_use)
            # Parsing fraction of resources to use
            string(REPLACE "!" ";" list_frtu ${frtu})
            list(GET list_frtu 0 frtu_name)
            list(GET list_frtu 1 frtu_value)

            # Some variables
            set(thistest_NAME "fewer-resources-${workload_name}-${platform_name}-${algo_name}-${frtu_name}")
            set(thistest_OUTPUT_DIR "${test_output_dir}/${thistest_NAME}")
            set(thistest_SCHED_INPUT_FILE "${thistest_OUTPUT_DIR}/sched_input.json")

            # Generate scheduler input
                file(MAKE_DIRECTORY ${thistest_OUTPUT_DIR})
                file(WRITE ${thistest_SCHED_INPUT_FILE}
"{\n\
    \"fraction_of_machines_to_use\": ${frtu_value},\n\
    \"custom_mapping\": false\n\
}"
                )

            add_test_robintest_batsched(NAME ${thistest_NAME}
                PLATFORM ${platform_filename}
                WORKLOAD ${workload_filename}
                ALGORITHM ${algo_realname}
                OTH_BATSIM_ARGS "--mmax-workload"
                OTH_BATSCHED_ARGS "--variant_options_filepath=${thistest_SCHED_INPUT_FILE}"
                ROBINTEST_EXPECTATION "--expect-ctx-clean --expect-ctx-clean-at-begin --expect-ctx-clean-at-end --expect-robin-success --expect-sched-success --expect-batsim-success"
                CHECK_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/check_duration_consistency.py"
            )
        endforeach()
    endforeach()
endforeach()
