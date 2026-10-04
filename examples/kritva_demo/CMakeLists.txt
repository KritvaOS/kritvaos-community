# Reference demo (KOS-I2): components + application as a library so tests can run it in-process.
add_library(kritva_demo_lib STATIC
    controller.cpp
    demo_app.cpp
    monitor.cpp
    sensor.cpp
)
target_include_directories(kritva_demo_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kritva_demo_lib PUBLIC kritva::runtime)

add_executable(kritva_demo main.cpp)
target_link_libraries(kritva_demo PRIVATE kritva_demo_lib)
