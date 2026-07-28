add_executable(code main.cpp
    module/fetch.cpp
    module/issue.cpp
    module/execute.cpp
    module/memory.cpp
    module/write_back.cpp
    module/commit.cpp
    module/RAT.cpp)

target_include_directories(code PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
