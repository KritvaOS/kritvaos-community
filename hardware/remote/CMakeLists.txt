# Nexus-side remote proxies and the Edge service (KOS-I4). Depends on the hardware abstraction and the transport layer only.
add_library(kritva_hardware_remote STATIC
    src/capability_dispatch.cpp
    src/edge_host.cpp
    src/remote_endpoint.cpp
    src/remote_node.cpp
    src/remote_session.cpp
    src/topology.cpp
)

target_compile_features(kritva_hardware_remote PUBLIC cxx_std_20)
target_include_directories(kritva_hardware_remote PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(kritva_hardware_remote PUBLIC kritva::hardware kritva::hardware_transport)

add_library(kritva::hardware_remote ALIAS kritva_hardware_remote)
