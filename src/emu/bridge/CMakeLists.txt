add_library(bridge
        include/bridge/arg_layout.h
        include/bridge/bridge.h
        include/bridge/bridge_types.h
        include/bridge/layout_args.h
        include/bridge/read_arg.h
        include/bridge/return_val.h
        include/bridge/vtab_lookup.h
        include/bridge/write_arg.h
        src/return_val.cpp)

target_link_libraries(bridge PUBLIC cpu common epocmem)

target_include_directories(bridge PUBLIC ${YAML_INCLUDE_DIR})
target_include_directories(bridge PUBLIC include ../epoc/include)
