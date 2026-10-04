# Mock Device / Endpoint implementations (KOS-I3), shared by tests and examples.
# Depends only on the hardware abstraction: no driver, bus or transport.
add_library(kritva_hardware_mock STATIC
    src/mock_endpoint.cpp
    src/mock_imu.cpp
    src/mock_motor.cpp
)

target_compile_features(kritva_hardware_mock PUBLIC cxx_std_20)
target_include_directories(kritva_hardware_mock PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(kritva_hardware_mock PUBLIC kritva::hardware)

add_library(kritva::hardware_mock ALIAS kritva_hardware_mock)
