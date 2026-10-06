# KritvaOS community tests (KOS-I2 runtime). Plain executables run through CTest,
# matching the kritva-core convention; no third-party test framework.
add_executable(kritva_runtime_host_test runtime/unit/runtime_host_test.cpp)
target_link_libraries(kritva_runtime_host_test PRIVATE kritva::runtime)
add_test(NAME kritva_runtime_host_unit COMMAND kritva_runtime_host_test)

add_executable(kritva_runtime_host_integration_test runtime/integration/runtime_host_integration_test.cpp)
target_link_libraries(kritva_runtime_host_integration_test PRIVATE kritva::runtime)
add_test(NAME kritva_runtime_host_integration COMMAND kritva_runtime_host_integration_test)

if(TARGET kritva_demo)
    add_test(NAME kritva_runtime_host_sanity
        COMMAND ${CMAKE_COMMAND} -DDEMO_EXE=$<TARGET_FILE:kritva_demo>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/runtime/sanity/run_demo_sanity.cmake)
endif()

# I2-002 component composition
add_executable(kritva_component_composition_test runtime/unit/component_composition_test.cpp)
target_link_libraries(kritva_component_composition_test PRIVATE kritva::runtime)
add_test(NAME kritva_component_composition_unit COMMAND kritva_component_composition_test)

add_executable(kritva_component_composition_integration_test runtime/integration/component_composition_integration_test.cpp)
target_link_libraries(kritva_component_composition_integration_test PRIVATE kritva::runtime)
add_test(NAME kritva_component_composition_integration COMMAND kritva_component_composition_integration_test)

add_executable(kritva_composition_sanity runtime/sanity/composition_sanity.cpp)
target_link_libraries(kritva_composition_sanity PRIVATE kritva::runtime)
add_test(NAME kritva_component_composition_sanity
    COMMAND ${CMAKE_COMMAND} -DSANITY_EXE=$<TARGET_FILE:kritva_composition_sanity>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/runtime/sanity/run_composition_sanity.cmake)

# I2-003 configuration
add_executable(kritva_configuration_test runtime/unit/configuration_test.cpp)
target_link_libraries(kritva_configuration_test PRIVATE kritva::runtime)
add_test(NAME kritva_configuration_unit COMMAND kritva_configuration_test)

add_executable(kritva_configuration_integration_test runtime/integration/configuration_integration_test.cpp)
target_link_libraries(kritva_configuration_integration_test PRIVATE kritva::runtime)
add_test(NAME kritva_configuration_integration COMMAND kritva_configuration_integration_test)

if(TARGET kritva_demo)
    add_test(NAME kritva_configuration_sanity
        COMMAND ${CMAKE_COMMAND} -DDEMO_EXE=$<TARGET_FILE:kritva_demo>
                -DDEMO_CONFIG=${CMAKE_SOURCE_DIR}/examples/kritva_demo/kritva_demo.conf
                -P ${CMAKE_CURRENT_SOURCE_DIR}/runtime/sanity/run_configuration_sanity.cmake)
endif()

# I2-004 runtime observation
add_executable(kritva_observation_test runtime/unit/observation_test.cpp)
target_link_libraries(kritva_observation_test PRIVATE kritva::runtime)
add_test(NAME kritva_observation_unit COMMAND kritva_observation_test)

add_executable(kritva_observation_integration_test runtime/integration/observation_integration_test.cpp)
target_link_libraries(kritva_observation_integration_test PRIVATE kritva::runtime)
add_test(NAME kritva_observation_integration COMMAND kritva_observation_integration_test)

add_executable(kritva_observation_sanity runtime/sanity/observation_sanity.cpp)
target_link_libraries(kritva_observation_sanity PRIVATE kritva::runtime)
add_test(NAME kritva_observation_sanity
    COMMAND ${CMAKE_COMMAND} -DSANITY_EXE=$<TARGET_FILE:kritva_observation_sanity>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/runtime/sanity/run_observation_sanity.cmake)

# I2-005 failure handling
add_executable(kritva_failure_handling_test runtime/unit/failure_handling_test.cpp)
target_link_libraries(kritva_failure_handling_test PRIVATE kritva::runtime)
add_test(NAME kritva_failure_handling_unit COMMAND kritva_failure_handling_test)

add_executable(kritva_failure_handling_integration_test runtime/integration/failure_handling_integration_test.cpp)
target_link_libraries(kritva_failure_handling_integration_test PRIVATE kritva::runtime)
add_test(NAME kritva_failure_handling_integration COMMAND kritva_failure_handling_integration_test)

add_executable(kritva_failure_sanity runtime/sanity/failure_sanity.cpp)
target_link_libraries(kritva_failure_sanity PRIVATE kritva::runtime)
add_test(NAME kritva_failure_handling_sanity
    COMMAND ${CMAKE_COMMAND} -DSANITY_EXE=$<TARGET_FILE:kritva_failure_sanity>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/runtime/sanity/run_failure_sanity.cmake)

# I2-006 reference demo
if(TARGET kritva_demo_lib)
    add_executable(kritva_demo_components_test runtime/unit/demo_components_test.cpp)
    target_link_libraries(kritva_demo_components_test PRIVATE kritva_demo_lib)
    add_test(NAME kritva_demo_components_unit COMMAND kritva_demo_components_test)

    add_executable(kritva_demo_application_test runtime/integration/demo_application_test.cpp)
    target_link_libraries(kritva_demo_application_test PRIVATE kritva_demo_lib)
    add_test(NAME kritva_demo_application_integration COMMAND kritva_demo_application_test)
endif()

if(TARGET kritva_demo)
    add_test(NAME kritva_demo_reference_sanity
        COMMAND ${CMAKE_COMMAND} -DDEMO_EXE=$<TARGET_FILE:kritva_demo>
                -DDEMO_CONFIG=${CMAKE_SOURCE_DIR}/examples/kritva_demo/kritva_demo.conf
                -DDEMO_FAILURE_CONFIG=${CMAKE_SOURCE_DIR}/examples/kritva_demo/kritva_demo_failure.conf
                -P ${CMAKE_CURRENT_SOURCE_DIR}/runtime/sanity/run_demo_reference_sanity.cmake)
endif()

# ------------------------------------------------------------------------------
# KOS-I3 hardware abstraction (Device / Endpoint contracts)
# ------------------------------------------------------------------------------
if(TARGET kritva_hardware)
    add_executable(kritva_hardware_identity_test hardware/unit/identity_test.cpp)
    target_link_libraries(kritva_hardware_identity_test PRIVATE kritva::hardware)
    add_test(NAME kritva_hardware_identity_unit COMMAND kritva_hardware_identity_test)

    add_executable(kritva_hardware_endpoint_test hardware/unit/endpoint_test.cpp)
    target_link_libraries(kritva_hardware_endpoint_test PRIVATE kritva::hardware)
    add_test(NAME kritva_hardware_endpoint_unit COMMAND kritva_hardware_endpoint_test)

    add_executable(kritva_hardware_device_test hardware/unit/device_test.cpp)
    target_link_libraries(kritva_hardware_device_test PRIVATE kritva::hardware)
    add_test(NAME kritva_hardware_device_unit COMMAND kritva_hardware_device_test)

    add_executable(kritva_hardware_registry_test hardware/unit/device_registry_test.cpp)
    target_link_libraries(kritva_hardware_registry_test PRIVATE kritva::hardware)
    add_test(NAME kritva_hardware_registry_unit COMMAND kritva_hardware_registry_test)

    add_executable(kritva_hardware_registry_integration_test hardware/integration/registry_discovery_integration_test.cpp)
    target_link_libraries(kritva_hardware_registry_integration_test PRIVATE kritva::hardware)
    add_test(NAME kritva_hardware_registry_integration COMMAND kritva_hardware_registry_integration_test)

    add_executable(kritva_hardware_manager_test hardware/unit/device_manager_test.cpp)
    target_link_libraries(kritva_hardware_manager_test PRIVATE kritva::hardware)
    add_test(NAME kritva_hardware_manager_unit COMMAND kritva_hardware_manager_test)

    add_executable(kritva_hardware_manager_integration_test hardware/integration/device_manager_runtime_integration_test.cpp)
    target_link_libraries(kritva_hardware_manager_integration_test PRIVATE kritva::hardware kritva::runtime)
    add_test(NAME kritva_hardware_manager_integration COMMAND kritva_hardware_manager_integration_test)

    add_executable(kritva_hardware_manager_sanity_exe hardware/sanity/device_manager_sanity.cpp)
    target_link_libraries(kritva_hardware_manager_sanity_exe PRIVATE kritva::hardware kritva::runtime)
    add_test(NAME kritva_hardware_manager_sanity
        COMMAND ${CMAKE_COMMAND} -DSANITY_EXE=$<TARGET_FILE:kritva_hardware_manager_sanity_exe>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/hardware/sanity/run_device_manager_sanity.cmake)

    add_executable(kritva_hardware_typed_test hardware/unit/typed_data_test.cpp)
    target_link_libraries(kritva_hardware_typed_test PRIVATE kritva::hardware)
    add_test(NAME kritva_hardware_typed_unit COMMAND kritva_hardware_typed_test)

    add_executable(kritva_hardware_typed_integration_test hardware/integration/typed_discovery_integration_test.cpp)
    target_link_libraries(kritva_hardware_typed_integration_test PRIVATE kritva::hardware)
    add_test(NAME kritva_hardware_typed_integration COMMAND kritva_hardware_typed_integration_test)

    add_executable(kritva_hardware_mock_test hardware/unit/mock_test.cpp)
    target_link_libraries(kritva_hardware_mock_test PRIVATE kritva::hardware_mock)
    add_test(NAME kritva_hardware_mock_unit COMMAND kritva_hardware_mock_test)

    add_executable(kritva_hardware_mock_integration_test hardware/integration/mock_runtime_integration_test.cpp)
    target_link_libraries(kritva_hardware_mock_integration_test PRIVATE kritva::hardware_mock kritva::runtime)
    add_test(NAME kritva_hardware_mock_integration COMMAND kritva_hardware_mock_integration_test)

    add_executable(kritva_hardware_mock_sanity_exe hardware/sanity/mock_sanity.cpp)
    target_link_libraries(kritva_hardware_mock_sanity_exe PRIVATE kritva::hardware_mock kritva::runtime)
    add_test(NAME kritva_hardware_mock_sanity
        COMMAND ${CMAKE_COMMAND} -DSANITY_EXE=$<TARGET_FILE:kritva_hardware_mock_sanity_exe>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/hardware/sanity/run_mock_sanity.cmake)

    # Sanity: the mock headers compile on their own, and the mock library depends only on the abstraction.
    set(KRITVA_MOCK_HEADERS mock_endpoint.hpp mock_imu.hpp mock_motor.hpp)
    set(KRITVA_MOCK_HEADER_CHECKS "")
    foreach(header IN LISTS KRITVA_MOCK_HEADERS)
        string(REPLACE "." "_" stem "${header}")
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/hardware_mock_header_checks/${stem}.cpp" "#include <kritva/hardware/mock/${header}>\n")
        list(APPEND KRITVA_MOCK_HEADER_CHECKS "${CMAKE_CURRENT_BINARY_DIR}/hardware_mock_header_checks/${stem}.cpp")
    endforeach()
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/hardware_mock_header_checks/main.cpp" "int main() { return 0; }\n")
    add_executable(kritva_hardware_mock_header_check ${KRITVA_MOCK_HEADER_CHECKS}
        "${CMAKE_CURRENT_BINARY_DIR}/hardware_mock_header_checks/main.cpp")
    target_link_libraries(kritva_hardware_mock_header_check PRIVATE kritva::hardware_mock)
    add_test(NAME kritva_hardware_mock_header_sanity COMMAND kritva_hardware_mock_header_check)
    get_target_property(KRITVA_MOCK_LINKS kritva_hardware_mock INTERFACE_LINK_LIBRARIES)
    if(NOT KRITVA_MOCK_LINKS STREQUAL "kritva::hardware")
        message(FATAL_ERROR "kritva_hardware_mock must depend only on kritva::hardware (found: ${KRITVA_MOCK_LINKS})")
    endif()

    add_executable(kritva_hardware_diagnostics_test hardware/unit/diagnostics_test.cpp)
    target_link_libraries(kritva_hardware_diagnostics_test PRIVATE kritva::hardware)
    add_test(NAME kritva_hardware_diagnostics_unit COMMAND kritva_hardware_diagnostics_test)

    add_executable(kritva_hardware_fault_integration_test hardware/integration/fault_observation_integration_test.cpp)
    target_link_libraries(kritva_hardware_fault_integration_test PRIVATE kritva::hardware_mock kritva::runtime)
    add_test(NAME kritva_hardware_fault_integration COMMAND kritva_hardware_fault_integration_test)

    add_executable(kritva_hardware_diagnostics_sanity_exe hardware/sanity/diagnostics_sanity.cpp)
    target_link_libraries(kritva_hardware_diagnostics_sanity_exe PRIVATE kritva::hardware_mock kritva::runtime)
    add_test(NAME kritva_hardware_diagnostics_sanity
        COMMAND ${CMAKE_COMMAND} -DSANITY_EXE=$<TARGET_FILE:kritva_hardware_diagnostics_sanity_exe>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/hardware/sanity/run_diagnostics_sanity.cmake)

    add_executable(kritva_hardware_runtime_integration_test hardware/integration/contract_runtime_integration_test.cpp)
    target_link_libraries(kritva_hardware_runtime_integration_test PRIVATE kritva::hardware kritva::runtime)
    add_test(NAME kritva_hardware_runtime_integration COMMAND kritva_hardware_runtime_integration_test)

    # Sanity: every public header compiles on its own (no hidden include dependencies).
    set(KRITVA_HARDWARE_HEADERS identity.hpp endpoint.hpp sensor_endpoint.hpp actuator_endpoint.hpp device.hpp device_registry.hpp device_manager.hpp samples.hpp typed_endpoints.hpp diagnostics.hpp)
    set(KRITVA_HARDWARE_HEADER_CHECKS "")
    foreach(header IN LISTS KRITVA_HARDWARE_HEADERS)
        string(REPLACE "." "_" stem "${header}")
        set(HEADER "${header}")
        set(generated "${CMAKE_CURRENT_BINARY_DIR}/hardware_header_checks/${stem}.cpp")
        configure_file(hardware/sanity/header_self_containment.cmake.in "${generated}" @ONLY)
        list(APPEND KRITVA_HARDWARE_HEADER_CHECKS "${generated}")
    endforeach()
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/hardware_header_checks/main.cpp" "int main() { return 0; }\n")
    add_executable(kritva_hardware_header_check ${KRITVA_HARDWARE_HEADER_CHECKS}
        "${CMAKE_CURRENT_BINARY_DIR}/hardware_header_checks/main.cpp")
    target_link_libraries(kritva_hardware_header_check PRIVATE kritva::hardware)
    add_test(NAME kritva_hardware_header_sanity COMMAND kritva_hardware_header_check)

    # Sanity: the abstraction depends only on kritva-core (no driver, bus or transport library).
    get_target_property(KRITVA_HARDWARE_LINKS kritva_hardware INTERFACE_LINK_LIBRARIES)
    if(NOT KRITVA_HARDWARE_LINKS STREQUAL "kritva_core::kritva_core")
        message(FATAL_ERROR "kritva_hardware must depend only on kritva_core (found: ${KRITVA_HARDWARE_LINKS})")
    endif()
endif()

# ------------------------------------------------------------------------------
# KOS-I3 reference device demo
# ------------------------------------------------------------------------------
if(TARGET kritva_device_demo_lib)
    add_executable(kritva_device_demo_test hardware/integration/device_demo_test.cpp)
    target_link_libraries(kritva_device_demo_test PRIVATE kritva_device_demo_lib)
    add_test(NAME kritva_device_demo_system COMMAND kritva_device_demo_test)

    add_test(NAME kritva_device_demo_sanity
        COMMAND ${CMAKE_COMMAND} -DDEMO_EXE=$<TARGET_FILE:kritva_device_demo>
                -DDEMO_CONFIG=${CMAKE_SOURCE_DIR}/examples/device_demo/device_demo.conf
                -DDEMO_CLEAN_CONFIG=${CMAKE_SOURCE_DIR}/examples/device_demo/device_demo_clean.conf
                -P ${CMAKE_CURRENT_SOURCE_DIR}/hardware/sanity/run_device_demo_sanity.cmake)
endif()

# ------------------------------------------------------------------------------
# KOS-I4 Nexus-Edge protocol contract (I4-001)
# ------------------------------------------------------------------------------
if(TARGET kritva_hardware_transport AND TARGET kritva_hardware_remote)
    add_executable(kritva_protocol_contract_test hardware/unit/protocol_contract_test.cpp)
    target_link_libraries(kritva_protocol_contract_test PRIVATE kritva::hardware_transport)
    add_test(NAME kritva_protocol_contract_unit COMMAND kritva_protocol_contract_test)

    add_executable(kritva_node_identity_test hardware/unit/node_identity_test.cpp)
    target_link_libraries(kritva_node_identity_test PRIVATE kritva::hardware_remote)
    add_test(NAME kritva_node_identity_unit COMMAND kritva_node_identity_test)

    add_executable(kritva_frame_test hardware/unit/frame_test.cpp)
    target_link_libraries(kritva_frame_test PRIVATE kritva::hardware_transport)
    add_test(NAME kritva_frame_unit COMMAND kritva_frame_test)

    add_executable(kritva_codec_test hardware/unit/codec_test.cpp)
    target_link_libraries(kritva_codec_test PRIVATE kritva::hardware_transport)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(kritva_codec_test PRIVATE -Wno-mismatched-new-delete)   # the test replaces operator new/delete to track allocations
    endif()
    add_test(NAME kritva_codec_unit COMMAND kritva_codec_test)

    add_executable(kritva_codec_truncation_test hardware/unit/codec_truncation_test.cpp)
    target_link_libraries(kritva_codec_truncation_test PRIVATE kritva::hardware_transport)
    add_test(NAME kritva_codec_truncation_unit COMMAND kritva_codec_truncation_test)

    add_executable(kritva_codec_fuzz_test hardware/unit/codec_fuzz_test.cpp)
    target_link_libraries(kritva_codec_fuzz_test PRIVATE kritva::hardware_transport)
    add_test(NAME kritva_codec_fuzz_unit COMMAND kritva_codec_fuzz_test)

    add_executable(kritva_simulated_transport_test hardware/unit/simulated_transport_test.cpp)
    target_link_libraries(kritva_simulated_transport_test PRIVATE kritva::hardware_transport)
    add_test(NAME kritva_simulated_transport_unit COMMAND kritva_simulated_transport_test)

    add_executable(kritva_link_config_test hardware/unit/link_config_test.cpp)
    target_link_libraries(kritva_link_config_test PRIVATE kritva::hardware_transport)
    add_test(NAME kritva_link_config_unit COMMAND kritva_link_config_test)

    add_executable(kritva_transport_hygiene_test hardware/unit/transport_hygiene_test.cpp)
    add_test(NAME kritva_transport_hygiene_unit COMMAND kritva_transport_hygiene_test ${CMAKE_SOURCE_DIR})

    add_executable(kritva_frame_transport_test hardware/integration/frame_transport_test.cpp)
    target_link_libraries(kritva_frame_transport_test PRIVATE kritva::hardware_transport)
    add_test(NAME kritva_frame_transport_integration COMMAND kritva_frame_transport_test)

    add_executable(kritva_edge_session_test hardware/unit/edge_session_test.cpp)
    target_link_libraries(kritva_edge_session_test PRIVATE kritva::hardware_remote kritva::hardware_mock)
    add_test(NAME kritva_edge_session_unit COMMAND kritva_edge_session_test)

    add_executable(kritva_edge_service_test hardware/unit/edge_service_test.cpp)
    target_link_libraries(kritva_edge_service_test PRIVATE kritva::hardware_remote kritva::hardware_mock)
    add_test(NAME kritva_edge_service_unit COMMAND kritva_edge_service_test)

    add_executable(kritva_edge_write_test hardware/unit/edge_write_test.cpp)
    target_link_libraries(kritva_edge_write_test PRIVATE kritva::hardware_remote kritva::hardware_mock)
    add_test(NAME kritva_edge_write_unit COMMAND kritva_edge_write_test)

    add_executable(kritva_capability_dispatch_test hardware/unit/capability_dispatch_test.cpp)
    target_link_libraries(kritva_capability_dispatch_test PRIVATE kritva::hardware_remote kritva::hardware_mock)
    add_test(NAME kritva_capability_dispatch_unit COMMAND kritva_capability_dispatch_test)

    add_executable(kritva_edge_transport_test hardware/integration/edge_transport_test.cpp)
    target_link_libraries(kritva_edge_transport_test PRIVATE kritva::hardware_remote kritva::hardware_mock)
    add_test(NAME kritva_edge_transport_integration COMMAND kritva_edge_transport_test)

    add_executable(kritva_remote_hygiene_test hardware/unit/remote_hygiene_test.cpp)
    add_test(NAME kritva_remote_hygiene_unit COMMAND kritva_remote_hygiene_test ${CMAKE_SOURCE_DIR})

    add_executable(kritva_topology_test hardware/unit/topology_test.cpp)
    target_link_libraries(kritva_topology_test PRIVATE kritva::hardware_remote)
    add_test(NAME kritva_topology_unit COMMAND kritva_topology_test)

    add_executable(kritva_remote_session_test hardware/unit/remote_session_test.cpp)
    target_link_libraries(kritva_remote_session_test PRIVATE kritva::hardware_remote kritva::hardware_mock)
    add_test(NAME kritva_remote_session_unit COMMAND kritva_remote_session_test)

    add_executable(kritva_remote_endpoint_test hardware/unit/remote_endpoint_test.cpp)
    target_link_libraries(kritva_remote_endpoint_test PRIVATE kritva::hardware_remote kritva::hardware_mock)
    add_test(NAME kritva_remote_endpoint_unit COMMAND kritva_remote_endpoint_test)

    add_executable(kritva_nexus_edge_test hardware/integration/nexus_edge_test.cpp)
    target_link_libraries(kritva_nexus_edge_test PRIVATE kritva::hardware_remote kritva::hardware_mock)
    add_test(NAME kritva_nexus_edge_integration COMMAND kritva_nexus_edge_test)

    add_executable(kritva_protocol_spec_consistency_test hardware/unit/protocol_spec_consistency_test.cpp)
    target_link_libraries(kritva_protocol_spec_consistency_test PRIVATE kritva::hardware_transport)
    add_test(NAME kritva_protocol_spec_consistency
        COMMAND kritva_protocol_spec_consistency_test ${CMAKE_SOURCE_DIR}/docs/architecture/KOS-I4_PROTOCOL.md)

    # Sanity: every public contract header compiles on its own.
    set(KRITVA_I4_HEADERS transport/protocol.hpp transport/frame.hpp transport/codec.hpp transport/transport.hpp transport/simulated_transport.hpp transport/link_config.hpp remote/node.hpp remote/capability_dispatch.hpp remote/edge_host.hpp remote/topology.hpp remote/remote_session.hpp remote/remote_node.hpp remote/remote_endpoint.hpp)
    set(KRITVA_I4_HEADER_CHECKS "")
    foreach(header IN LISTS KRITVA_I4_HEADERS)
        string(REPLACE "/" "_" stem "${header}")
        string(REPLACE "." "_" stem "${stem}")
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/i4_header_checks/${stem}.cpp" "#include <kritva/hardware/${header}>\n")
        list(APPEND KRITVA_I4_HEADER_CHECKS "${CMAKE_CURRENT_BINARY_DIR}/i4_header_checks/${stem}.cpp")
    endforeach()
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/i4_header_checks/main.cpp" "int main() { return 0; }\n")
    add_executable(kritva_i4_header_check ${KRITVA_I4_HEADER_CHECKS} "${CMAKE_CURRENT_BINARY_DIR}/i4_header_checks/main.cpp")
    target_link_libraries(kritva_i4_header_check PRIVATE kritva::hardware_remote)
    add_test(NAME kritva_i4_header_sanity COMMAND kritva_i4_header_check)

    # Sanity: the I4 layers depend only on what the architecture allows.
    get_target_property(KRITVA_TRANSPORT_LINKS kritva_hardware_transport INTERFACE_LINK_LIBRARIES)
    if(NOT KRITVA_TRANSPORT_LINKS STREQUAL "kritva_core::kritva_core")
        message(FATAL_ERROR "kritva_hardware_transport must depend only on kritva_core (found: ${KRITVA_TRANSPORT_LINKS})")
    endif()
    get_target_property(KRITVA_REMOTE_LINKS kritva_hardware_remote INTERFACE_LINK_LIBRARIES)
    if(NOT KRITVA_REMOTE_LINKS STREQUAL "kritva::hardware;kritva::hardware_transport")
        message(FATAL_ERROR "kritva_hardware_remote must depend only on kritva::hardware and kritva::hardware_transport (found: ${KRITVA_REMOTE_LINKS})")
    endif()
endif()
