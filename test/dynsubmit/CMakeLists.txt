# Simulation instances
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_delay1}")
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_mixed}")

# Algorithms
list(APPEND algos ${BSALGO_submitter})

# Submitter parameters.
# Syntax: "name!increase_jobs_duration!send_profile_if_already_sent!send_profiles_in_separate_event"
list(APPEND subparams "diff-profiles-separatep!true!true!true")
list(APPEND subparams "same-profiles-noresend-separatep!false!false!true")

# Whether redis should be enabled
# Syntax: "name!redis_mode"
list(APPEND redis_modes "redis!true")
list(APPEND redis_modes "noredis!false")

# Whether dynamic submissions should be acknowledged
# Syntax: "name!dynamic_submission_acknowledge_mode"
list(APPEND dynsubmit_ack_modes "ack!true")
list(APPEND dynsubmit_ack_modes "noack!false")

# Number of dynamic jobs to submit
list(APPEND nb_dynamic_jobs "0jobs!0")
list(APPEND nb_dynamic_jobs "1jobs!1")
list(APPEND nb_dynamic_jobs "10jobs!10")


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
    set(algo_name "submitter")
    set(algo_realname "submitter")

    foreach(subparam IN LISTS subparams)
        # Parsing subparam
        string(REPLACE "!" ";" list_subparam ${subparam})
        list(GET list_subparam 0 subparam_name)
        list(GET list_subparam 1 subparam_increase_jobs_duration)
        list(GET list_subparam 2 subparam_send_profile_if_already_sent)
        list(GET list_subparam 3 subparam_send_profiles_in_separate_event)

        foreach(redis_mode IN LISTS redis_modes)
            # Parsing redis_mode
            string(REPLACE "!" ";" list_redis_mode ${redis_mode})
            list(GET list_redis_mode 0 redis_mode_name)
            list(GET list_redis_mode 1 redis_mode_value)

            foreach(dsam IN LISTS dynsubmit_ack_modes)
                # Parsing dynsubmit_ack_mode
                string(REPLACE "!" ";" list_dsam ${dsam})
                list(GET list_dsam 0 dsam_name)
                list(GET list_dsam 1 dsam_value)

                foreach(nb_djobs IN LISTS nb_dynamic_jobs)
                    # Parse nb_dynamic_jobs
                    string(REPLACE "!" ";" list_nb_djobs ${nb_djobs})
                    list(GET list_nb_djobs 0 nb_djobs_name)
                    list(GET list_nb_djobs 1 nb_djobs_value)

                    # Finally!!
                    # Some variables
                    set(thistest_NAME "dynsubmit-${workload_name}-${platform_name}-${subparam_name}-${redis_mode_name}-${dsam_name}-${nb_djobs_name}")

                    set(thistest_OUTPUT_DIR "${test_output_dir}/${thistest_NAME}")
                    set(thistest_BATSIM_CONFIG_FILE "${thistest_OUTPUT_DIR}/batconf.json")
                    set(thistest_SCHED_INPUT_FILE "${thistest_OUTPUT_DIR}/sched_input.json")

                    set(thistest_OTH_BATSIM_ARGS "--enable-dynamic-jobs")
                    if (${redis_mode_value} EQUAL "true")
                        set(thistest_OTH_BATSIM_ARGS "${thistest_OTH_BATSIM_ARGS} --enable-redis")
                    endif()
                    if (${dsam_value} EQUAL "true")
                        set(thistest_OTH_BATSIM_ARGS "${thistest_OTH_BATSIM_ARGS} --acknowledge-dynamic-jobs")
                    endif()

                    # Generate batsim configuration file
                    file(MAKE_DIRECTORY ${thistest_OUTPUT_DIR})

                    # Generate batsched input file
                    file(WRITE ${thistest_SCHED_INPUT_FILE}
"{\n\
  \"nb_jobs_to_submit\": ${nb_djobs_value},\n\
  \"increase_jobs_duration\": ${subparam_increase_jobs_duration},\n\
  \"send_profile_if_already_sent\": ${subparam_send_profile_if_already_sent},\n\
  \"send_profiles_in_separate_event\": ${subparam_send_profiles_in_separate_event}\n\
}"
                    )

                    # Create the test
                    add_test_robintest_batsched(NAME ${thistest_NAME}
                        PLATFORM ${platform_filename}
                        WORKLOAD ${workload_filename}
                        ALGORITHM ${algo_realname}
                        OTH_BATSIM_ARGS "${thistest_OTH_BATSIM_ARGS}"
                        OTH_BATSCHED_ARGS "--variant_options_filepath=${thistest_SCHED_INPUT_FILE}"
                        ROBINTEST_EXPECTATION "--expect-ctx-clean --expect-robin-success --expect-sched-success --expect-batsim-success"
                    )
                endforeach()
            endforeach()
        endforeach()
    endforeach()
endforeach()
