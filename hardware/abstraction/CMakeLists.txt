# Device / Endpoint contracts (KOS-I3). Depends only on kritva-core: no driver, bus or transport.
add_library(kritva_hardware STATIC
    src/device.cpp
    src/device_registry.cpp
    src/endpoint.cpp
)

target_compile_features(kritva_hardware PUBLIC cxx_std_20)
target_include_directories(kritva_hardware PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(kritva_hardware PUBLIC kritva_core::kritva_core)

add_library(kritva::hardware ALIAS kritva_hardware)
