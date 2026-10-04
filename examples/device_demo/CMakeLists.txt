# KOS-I3 reference device demo: application as a library so tests can run it in-process.
add_library(kritva_device_demo_lib STATIC device_demo_app.cpp)
target_include_directories(kritva_device_demo_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kritva_device_demo_lib PUBLIC kritva::hardware_mock kritva::runtime)

add_executable(kritva_device_demo main.cpp)
target_link_libraries(kritva_device_demo PRIVATE kritva_device_demo_lib)
