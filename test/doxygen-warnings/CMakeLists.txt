# Create the test
add_test(NAME doxygen-warnings
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/check_no_warnings.py
            ${CMAKE_SOURCE_DIR}/doc
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
