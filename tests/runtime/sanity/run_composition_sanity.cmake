# Sanity: three composed components start in dependency order and stop in reverse.
execute_process(COMMAND ${SANITY_EXE} RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
message("${out}")
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "composition sanity exited with ${rc}: ${err}")
endif()
set(expected
    "third.start:READY" "second.start:READY" "first.start:READY"
    "first.stop:STOPPING" "second.stop:STOPPING" "third.stop:STOPPING")
set(pos 0)
foreach(token IN LISTS expected)
    string(FIND "${out}" "${token}" idx)
    if(idx LESS 0 OR idx LESS pos)
        message(FATAL_ERROR "sanity: '${token}' missing or out of order")
    endif()
    string(LENGTH "${token}" len)
        math(EXPR pos "${idx} + ${len}")
endforeach()
