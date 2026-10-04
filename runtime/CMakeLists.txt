# KritvaOS application runtime layer (KOS-I2). Depends only on kritva-core.
add_library(kritva_runtime STATIC
    src/configuration.cpp
    src/runtime_host.cpp
)

target_compile_features(kritva_runtime PUBLIC cxx_std_20)
target_include_directories(kritva_runtime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(kritva_runtime PUBLIC kritva_core::kritva_core)

add_library(kritva::runtime ALIAS kritva_runtime)
