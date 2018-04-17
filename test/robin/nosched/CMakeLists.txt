# Simulation instances
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_compute1}")
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_delay1}")
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_verylong}")
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_delay_sequences}")
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_delay_same_profiles}")
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_tuto1}")
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_smpi_compute1}")
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_smpi_compute2}")
list(APPEND simu_instances "${SGPLATFORM_small}?${BATWLOAD_smpi_mixed}")
list(APPEND simu_instances "${SGPLATFORM_cluster512}?${BATWLOAD_delays}")
list(APPEND simu_instances "${SGPLATFORM_cluster512}?${BATWLOAD_long}")

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

    # Variables
    set(thistest_NAME "nosched-${workload_name}-${platform_name}")
    set(thistest_OUTPUT_DIR "${test_output_dir}/${thistest_NAME}")

    add_test_robintest(NAME ${thistest_NAME}
        OUTPUT_DIR ${thistest_OUTPUT_DIR}
        BATCMD "batsim -p ${platform_filename} -w ${workload_filename} -e ${thistest_OUTPUT_DIR}/out --batexec"
        SCHEDCMD ""
        ROBINTEST_EXPECTATION "--expect-ctx-clean --expect-ctx-clean-at-begin --expect-ctx-clean-at-end --expect-robin-success --expect-no-sched --expect-batsim-success"
    )
endforeach()
