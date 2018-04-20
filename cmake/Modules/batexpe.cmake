# The user can include this file.
#
# Once done, the following will be defined:
# - robin_COMMAND
# - robintest_COMMAND
#
# If the commands are not found, the value of the variables will be set to:
# - robin_COMMAND-NOTFOUND
# - robintest_COMMAND-NOTFOUND

find_program(robin_COMMAND
    NAMES robin
    PATHS ENV PATH)

find_program(robintest_COMMAND
    NAMES robintest
    PATHS ENV PATH)
