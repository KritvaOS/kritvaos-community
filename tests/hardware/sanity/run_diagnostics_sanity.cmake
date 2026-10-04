# Sanity: device and endpoint status, health, statistics, capability and the fault evidence are printed.
execute_process(COMMAND ${SANITY_EXE} RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
message("${out}")
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "diagnostics sanity exited with ${rc}: ${err}")
endif()
set(expected
    "[diag] running"
    "device left_arm_imu (id=1) enabled=true status=OK health=HEALTHY capabilities=acceleration,angular_velocity"
    "endpoint acceleration (id=1) sensor state=RUNNING status=OK health=HEALTHY ok=1 failed=0 capabilities=acceleration"
    "[diag] faulted"
    "device left_arm_imu (id=1) enabled=true status=FAILED health=UNHEALTHY detail=\"angular_velocity: gyro lost\""
    "endpoint angular_velocity (id=2) sensor state=FAULT status=FAILED health=UNHEALTHY detail=\"gyro lost\" ok=0 failed=0 capabilities=angular_velocity last_error=INTERNAL_ERROR \"gyro lost\""
    "[diag] shutdown"
    "endpoint angular_velocity (id=2) sensor state=STOPPED status=OK health=UNKNOWN")
set(pos 0)
foreach(token IN LISTS expected)
    string(FIND "${out}" "${token}" idx)
    if(idx LESS 0 OR idx LESS pos)
        message(FATAL_ERROR "sanity: '${token}' missing or out of order")
    endif()
    string(LENGTH "${token}" len)
    math(EXPR pos "${idx} + ${len}")
endforeach()
