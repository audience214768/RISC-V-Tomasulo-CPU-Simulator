add_executable(code main.cpp
    core/cpu_top.cpp
    storage/prf_module.cpp
    storage/ready_table_module.cpp
    storage/rat_module.cpp
    storage/free_list_module.cpp
    storage/cdb_module.cpp
    storage/rs_module.cpp
    storage/rob_module.cpp
    storage/lsq_module.cpp
    storage/fetch_module.cpp
    storage/bht_module.cpp
    storage/ras_module.cpp)

target_include_directories(code PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

set_target_properties(code PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)


