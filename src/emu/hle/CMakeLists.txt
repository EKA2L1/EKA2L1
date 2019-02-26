add_library(hle 
    include/hle/arg_layout.h
    include/hle/bridge.h
    include/hle/bridge_types.h
    include/hle/layout_args.h
    include/hle/read_arg.h
    include/hle/return_val.h
    include/hle/vtab_lookup.h
    include/hle/write_arg.h
    src/return_val.cpp)

target_link_libraries(hle PUBLIC arm common)

target_include_directories(hle PUBLIC ${YAML_INCLUDE_DIR})

# The epoc inclusion is for getting the system header
target_include_directories(hle PUBLIC include ../epoc/include)
