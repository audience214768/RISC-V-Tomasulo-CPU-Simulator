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

# Unit test for instruction decode
add_executable(test_decode
    test_decode.cpp
    module/issue.cpp)

target_include_directories(test_decode PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

set_target_properties(test_decode PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
