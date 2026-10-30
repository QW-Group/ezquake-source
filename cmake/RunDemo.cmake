# Plays a demo with timedemo and quits when it ends, so instrumented builds can be
# exercised through CTest. Invoked by the timedemo test with -DEZQUAKE=<executable>.
#
# The demo is picked when the test runs, from the environment:
#
#   EZQUAKE_BASEDIR  Quake directory containing id1/ and qw/
#   EZQUAKE_DEMO     demo name as given to the timedemo command
#   EZQUAKE_ARGS     optional extra command line arguments
#   EZQUAKE_TIMEOUT  optional timeout in seconds, defaults to 600
#
# The test is reported as skipped when EZQUAKE_BASEDIR or EZQUAKE_DEMO is unset.

if(NOT EZQUAKE)
    message(FATAL_ERROR "EZQUAKE must be set to the ezquake executable")
endif()

if("$ENV{EZQUAKE_BASEDIR}" STREQUAL "" OR "$ENV{EZQUAKE_DEMO}" STREQUAL "")
    message("SKIPPED: set EZQUAKE_BASEDIR and EZQUAKE_DEMO to play a demo")
    return()
endif()

file(TO_CMAKE_PATH "$ENV{EZQUAKE_BASEDIR}" basedir)
if(NOT IS_DIRECTORY "${basedir}")
    message(FATAL_ERROR "EZQUAKE_BASEDIR is not a directory: ${basedir}")
endif()

set(timeout "$ENV{EZQUAKE_TIMEOUT}")
if(timeout STREQUAL "")
    set(timeout 600)
endif()

separate_arguments(extra_args NATIVE_COMMAND "$ENV{EZQUAKE_ARGS}")

# The client has no usable stdout on all platforms, so the console is captured
# through -condebug, which appends to qw/qconsole.log in the basedir.
set(console_log "${basedir}/qw/qconsole.log")
set(console_offset 0)
if(EXISTS "${console_log}")
    file(SIZE "${console_log}" console_offset)
endif()

# Command line + commands run after the configs, so the trigger and settings here
# take precedence over the user's own. f_demoend fires when playback stops.
execute_process(
        COMMAND "${EZQUAKE}"
                -basedir "${basedir}"
                -window
                -condebug
                ${extra_args}
                +set cfg_save_onquit 0
                +set tp_triggers 1
                +set cl_restrictions 0
                +alias f_demoend quit
                +timedemo "$ENV{EZQUAKE_DEMO}"
        RESULT_VARIABLE result
        TIMEOUT ${timeout}
)

if(EXISTS "${console_log}")
    file(READ "${console_log}" console_output OFFSET ${console_offset})
    message("${console_output}")
endif()

if(NOT result STREQUAL "0")
    message(FATAL_ERROR "ezquake exited with: ${result}")
endif()
