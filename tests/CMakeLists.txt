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
