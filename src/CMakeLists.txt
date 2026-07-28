add_executable(code main.cpp
    core/cpu_simulator.cpp
    module/fetch.cpp
    module/issue.cpp
    module/execute.cpp
    module/memory.cpp
    module/write_back.cpp
    module/commit.cpp)

target_include_directories(code PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

set_target_properties(code PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
