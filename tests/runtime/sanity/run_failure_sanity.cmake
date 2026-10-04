# Sanity: injected failure -> FAULT/UNHEALTHY -> ERROR event -> failure report -> controlled shutdown.
execute_process(COMMAND ${SANITY_EXE} RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
message("${out}")
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "failure sanity exited with ${rc}: ${err}")
endif()
set(expected
    "[failure] RUNNING"
    "component sensor (id=1) state=FAULT health=UNHEALTHY"
    "[failure] failed=1 affected=1"
    "[failure] event ERROR source=1"
    "[failure] runtime state=STOPPED")
set(pos 0)
foreach(token IN LISTS expected)
    string(FIND "${out}" "${token}" idx)
    if(idx LESS 0 OR idx LESS pos)
        message(FATAL_ERROR "sanity: '${token}' missing or out of order")
    endif()
    string(LENGTH "${token}" len)
        math(EXPR pos "${idx} + ${len}")
endforeach()
