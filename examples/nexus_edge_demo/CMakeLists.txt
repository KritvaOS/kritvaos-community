# KOS-I4 Nexus-Edge reference demo: application as a library so tests can run it in-process.
add_library(kritva_nexus_edge_demo_lib STATIC nexus_edge_demo_app.cpp)
target_include_directories(kritva_nexus_edge_demo_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kritva_nexus_edge_demo_lib PUBLIC kritva::hardware_remote kritva::hardware_mock kritva::runtime)

add_executable(kritva_nexus_edge_demo main.cpp)
target_link_libraries(kritva_nexus_edge_demo PRIVATE kritva_nexus_edge_demo_lib)
