# Sanity: runtime state, component state, health, statistics and events are observable.
execute_process(COMMAND ${SANITY_EXE} RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
message("${out}")
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "observation sanity exited with ${rc}: ${err}")
endif()
foreach(token
        "runtime state=READY"
        "runtime state=RUNNING"
        "runtime state=STOPPED"
        "component b (id=2) state=RUNNING health=HEALTHY"
        "component a (id=1) state=RUNNING health=HEALTHY"
        "ok_ops=4 failed_ops=0"
        "events=2")
    string(FIND "${out}" "${token}" idx)
    if(idx LESS 0)
        message(FATAL_ERROR "sanity: '${token}' not observed")
    endif()
endforeach()
