add_executable(code main.cpp
    core/cpu_top.cpp)

target_include_directories(code PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

set_target_properties(code PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)


