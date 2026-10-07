# End-to-end sanity of the kritva_nexus_edge_demo executable: the full scenario, the clean scenario, determinism, an invalid
# configuration, a misspelled key and the default run.
function(run_demo expected_rc)
    execute_process(COMMAND ${DEMO_EXE} ${ARGN} RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    set(DEMO_OUTPUT "${out}" PARENT_SCOPE)
    message("${out}")
    if(NOT rc EQUAL ${expected_rc})
        message(FATAL_ERROR "kritva_nexus_edge_demo ${ARGN}: exit ${rc}, expected ${expected_rc}. ${err}")
    endif()
endfunction()

function(expect_in_order out)
    set(pos 0)
    foreach(token IN LISTS ARGN)
        string(FIND "${out}" "${token}" idx)
        if(idx LESS 0 OR idx LESS pos)
            message(FATAL_ERROR "nexus edge demo: '${token}' missing or out of order")
        endif()
        string(LENGTH "${token}" len)
        math(EXPR pos "${idx} + ${len}")
    endforeach()
endfunction()

run_demo(0 ${DEMO_CONFIG})
set(first "${DEMO_OUTPUT}")
expect_in_order("${first}"
    "HELLO accepted, discovery complete: 2 remote devices" "state=RUNNING" "motor command 0.500 rad/s accepted by the Edge"
    "both sides kept sending heartbeats" "hostile link: duplicated and replayed frames" "injecting link loss"
    "heartbeat timeout expired: it stopped its own actuator" "fault_origin=LINK_LOST" "no reconnect, nothing sent"
    "a fresh session 3 (was 2)" "everything is STOPPED, on the Nexus and on the Edge" "controlled shutdown complete (error events=4)" "RESULT: PASS")

# Determinism: the same configuration always gives the same output (no wall clock, no threads).
run_demo(0 ${DEMO_CONFIG})
if(NOT "${DEMO_OUTPUT}" STREQUAL "${first}")
    message(FATAL_ERROR "the demo output is not reproducible")
endif()

run_demo(0 ${DEMO_CLEAN_CONFIG})
expect_in_order("${DEMO_OUTPUT}" "link timing: heartbeat 50 ms, timeout 200 ms" "injecting link loss" "RESULT: PASS")
string(FIND "${DEMO_OUTPUT}" "hostile link" idx)
if(NOT idx LESS 0)
    message(FATAL_ERROR "the clean run ran the hostile phase")
endif()

set(bad "${CMAKE_CURRENT_BINARY_DIR}/nexus_edge_demo_invalid.conf")
file(WRITE "${bad}" "runtime.name=demo\nlink.heartbeat_period_ms=200\nlink.heartbeat_timeout_ms=300\n")
run_demo(1 "${bad}")
string(FIND "${DEMO_OUTPUT}" "HELLO accepted" idx)
if(NOT idx LESS 0)
    message(FATAL_ERROR "the link was used despite an invalid timing")
endif()

set(typo "${CMAKE_CURRENT_BINARY_DIR}/nexus_edge_demo_typo.conf")
file(WRITE "${typo}" "runtime.name=demo\nshoulder_motor.command.max_rad=2\n")
run_demo(1 "${typo}")
string(FIND "${DEMO_OUTPUT}" "unknown key 'shoulder_motor.command.max_rad'" idx)
if(idx LESS 0)
    message(FATAL_ERROR "the misspelled key was not reported")
endif()

run_demo(0)
expect_in_order("${DEMO_OUTPUT}" "RESULT: PASS")
