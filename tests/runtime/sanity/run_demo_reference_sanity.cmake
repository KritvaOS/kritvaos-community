# End-to-end sanity of the reference demo executable: normal run and failure run.
function(run_demo config expected_rc)
    execute_process(COMMAND ${DEMO_EXE} ${config} RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    message("${out}")
    if(NOT rc EQUAL ${expected_rc})
        message(FATAL_ERROR "kritva_demo ${config}: exit ${rc}, expected ${expected_rc}. ${err}")
    endif()
    set(pos 0)
    foreach(token IN LISTS ARGN)
        string(FIND "${out}" "${token}" idx)
        if(idx LESS 0 OR idx LESS pos)
            message(FATAL_ERROR "kritva_demo ${config}: '${token}' missing or out of order")
        endif()
        string(LENGTH "${token}" len)
        math(EXPR pos "${idx} + ${len}")
    endforeach()
endfunction()

run_demo(${DEMO_CONFIG} 0
    "composed: sensor controller monitor" "state=READY" "state=RUNNING" "tick 5 sensor=50 command=100 failed=0"
    "component sensor (id=1) state=RUNNING health=HEALTHY" "state=STOPPED" "shutdown complete")

run_demo(${DEMO_FAILURE_CONFIG} 3
    "state=RUNNING" "tick 3 sensor=30" "component sensor (id=1) state=FAULT health=UNHEALTHY"
    "FAILURE observed: failed=sensor affected=controller,monitor" "event ERROR source=1"
    "state=STOPPED" "controlled shutdown complete")

# The default (no config file) run is also clean.
execute_process(COMMAND ${DEMO_EXE} RESULT_VARIABLE rc OUTPUT_VARIABLE out)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "kritva_demo without config exited with ${rc}")
endif()
