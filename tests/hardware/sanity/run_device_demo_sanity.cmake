# End-to-end sanity of the kritva_device_demo executable: the fault scenario, the clean scenario,
# an invalid configuration, a misspelled key and the default run.
function(run_demo expected_rc)
    execute_process(COMMAND ${DEMO_EXE} ${ARGN} RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    set(DEMO_OUTPUT "${out}" PARENT_SCOPE)
    message("${out}")
    if(NOT rc EQUAL ${expected_rc})
        message(FATAL_ERROR "kritva_device_demo ${ARGN}: exit ${rc}, expected ${expected_rc}. ${err}")
    endif()
endfunction()

function(expect_in_order out)
    set(pos 0)
    foreach(token IN LISTS ARGN)
        string(FIND "${out}" "${token}" idx)
        if(idx LESS 0 OR idx LESS pos)
            message(FATAL_ERROR "device demo: '${token}' missing or out of order")
        endif()
        string(LENGTH "${token}" len)
        math(EXPR pos "${idx} + ${len}")
    endforeach()
endfunction()

run_demo(0 ${DEMO_CONFIG})
expect_in_order("${DEMO_OUTPUT}"
    "registered devices: 2" "discovered shoulder_motor.position" "state=RUNNING" "motor command 0.500 rad/s accepted"
    "tick 1" "injecting a fault into left_arm_imu.angular_velocity" "FAILURE observed: failed=device_manager"
    "endpoint angular_velocity (id=2) sensor state=FAULT" "no silent recovery" "state=STOPPED"
    "controlled shutdown after the fault complete (error events=1)")

run_demo(0 ${DEMO_CLEAN_CONFIG})
expect_in_order("${DEMO_OUTPUT}" "state=RUNNING" "tick 6" "state=STOPPED" "clean shutdown complete (error events=0)")
string(FIND "${DEMO_OUTPUT}" "FAILURE" idx)
if(NOT idx LESS 0)
    message(FATAL_ERROR "the clean run reported a failure")
endif()

set(bad "${CMAKE_CURRENT_BINARY_DIR}/device_demo_invalid.conf")
file(WRITE "${bad}" "runtime.name=demo\ndemo.ticks=0\n")
run_demo(1 "${bad}")
string(FIND "${DEMO_OUTPUT}" "state=READY" idx)
if(NOT idx LESS 0)
    message(FATAL_ERROR "the lifecycle ran despite an invalid configuration")
endif()

set(typo "${CMAKE_CURRENT_BINARY_DIR}/device_demo_typo.conf")
file(WRITE "${typo}" "runtime.name=demo\nshoulder_motor.command.max_rad=2\n")
run_demo(1 "${typo}")
string(FIND "${DEMO_OUTPUT}" "unknown key 'shoulder_motor.command.max_rad'" idx)
if(idx LESS 0)
    message(FATAL_ERROR "the misspelled key was not reported")
endif()

run_demo(0)
