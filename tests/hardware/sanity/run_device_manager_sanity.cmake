# Sanity: manager lifecycle, an endpoint fault that the runtime observes, and a controlled shutdown.
execute_process(COMMAND ${SANITY_EXE} RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
message("${out}")
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "device manager sanity exited with ${rc}: ${err}")
endif()
set(expected
    "[manager] initialized: runtime=READY endpoint=READY"
    "[manager] started: runtime=RUNNING endpoint=RUNNING"
    "[manager] faulted: runtime=RUNNING endpoint=FAULT"
    "[manager] failure_report failed=1"
    "[manager] shutdown: runtime=STOPPED endpoint=STOPPED")
set(pos 0)
foreach(token IN LISTS expected)
    string(FIND "${out}" "${token}" idx)
    if(idx LESS 0 OR idx LESS pos)
        message(FATAL_ERROR "sanity: '${token}' missing or out of order")
    endif()
    string(LENGTH "${token}" len)
    math(EXPR pos "${idx} + ${len}")
endforeach()
