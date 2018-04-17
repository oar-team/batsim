# Simulation instances
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_compute1}")
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_delay1}")
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_mixed}")

# The number of kills to do for each job
list(APPEND nb_kills_per_job "1kill!1")

# The delay before killing each job
list(APPEND delay_before_kill "after0!0")
list(APPEND delay_before_kill "after5!5")

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

    # Setting algorithm
    set(algo_name "killer")
    set(algo_realname "killer")

    foreach(nb_kpj IN LISTS nb_kills_per_job)
        # Parsing nb kills per job
        string(REPLACE "!" ";" list_nb_kpj ${nb_kpj})
        list(GET list_nb_kpj 0 nb_kpj_name)
        list(GET list_nb_kpj 1 nb_kpj_value)

        foreach(delay_bk IN LISTS delay_before_kill)
            # Parsing nb kills per job
            string(REPLACE "!" ";" list_delay_bk ${delay_bk})
            list(GET list_delay_bk 0 delay_bk_name)
            list(GET list_delay_bk 1 delay_bk_value)

            # Some variables
            set(thistest_NAME "kill-progress-${workload_name}-${platform_name}-${nb_kpj_name}-${delay_bk_name}")
            set(thistest_OUTPUT_DIR "${test_output_dir}/${thistest_NAME}")
            set(thistest_SCHED_INPUT_FILE "${thistest_OUTPUT_DIR}/sched_input.json")

            # Generate batsim configuration file
            file(MAKE_DIRECTORY ${thistest_OUTPUT_DIR})
            file(WRITE ${thistest_SCHED_INPUT_FILE}
"{\n\
  \"nb_kills_per_job\": ${nb_kpj_value},\n\
  \"delay_before_kill\": ${delay_bk_value}\n\
}"
            )

            # Create the test
            add_test_robintest_batsched(NAME ${thistest_NAME}
                PLATFORM ${platform_filename}
                WORKLOAD ${workload_filename}
                ALGORITHM ${algo_realname}
                OTH_BATSIM_ARGS ""
                OTH_BATSCHED_ARGS "--variant_options_filepath=${thistest_SCHED_INPUT_FILE}"
                ROBINTEST_EXPECTATION "--expect-ctx-clean --expect-ctx-clean-at-begin --expect-ctx-clean-at-end --expect-robin-success --expect-sched-success --expect-batsim-success"
                CHECK_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/caller.bash"
                WORKING_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
            )
        endforeach()
    endforeach()
endforeach()
