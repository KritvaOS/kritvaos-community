# Nexus-Edge protocol and transport layer (KOS-I4). Depends only on kritva-core.
add_library(kritva_hardware_transport STATIC
    src/codec.cpp
    src/frame.cpp
    src/link_config.cpp
    src/protocol.cpp
    src/simulated_transport.cpp
)

target_compile_features(kritva_hardware_transport PUBLIC cxx_std_20)
target_include_directories(kritva_hardware_transport PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(kritva_hardware_transport PUBLIC kritva_core::kritva_core)

add_library(kritva::hardware_transport ALIAS kritva_hardware_transport)
