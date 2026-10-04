# Sanity: deterministic mock data, actuator validation, fault observation and controlled shutdown.
execute_process(COMMAND ${SANITY_EXE} RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
message("${out}")
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "mock sanity exited with ${rc}: ${err}")
endif()
set(expected
    "[mock] imu n=1 accel=(0.0,-0.0,9.81) gyro_x=0.0 t=1000000ns"
    "[mock] imu n=2 accel=(1.0,-1.0,9.81) gyro_x=1.0 t=2000000ns"
    "[mock] imu n=3 accel=(2.0,-2.0,9.81) gyro_x=2.0 t=3000000ns"
    "[mock] motor write 0.5 ok, write 5.0 rejected=1"
    "[mock] position=0.005"
    "[mock] failure_report failed=1"
    "[mock] shutdown state=STOPPED")
set(pos 0)
foreach(token IN LISTS expected)
    string(FIND "${out}" "${token}" idx)
    if(idx LESS 0 OR idx LESS pos)
        message(FATAL_ERROR "sanity: '${token}' missing or out of order")
    endif()
    string(LENGTH "${token}" len)
    math(EXPR pos "${idx} + ${len}")
endforeach()
