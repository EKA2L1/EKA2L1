add_library(hle 
    ${CORE_INCLUDE_DIR}/hle/libmanager.h
    ${CORE_INCLUDE_DIR}/hle/arg_layout.h
    ${CORE_INCLUDE_DIR}/hle/bridge.h
    ${CORE_INCLUDE_DIR}/hle/bridge_types.h
    ${CORE_INCLUDE_DIR}/hle/layout_args.h
    ${CORE_INCLUDE_DIR}/hle/read_arg.h
    ${CORE_INCLUDE_DIR}/hle/return_val.h
    ${CORE_INCLUDE_DIR}/hle/vtab_lookup.h
    ${CORE_INCLUDE_DIR}/hle/write_arg.h
    libmanager.cpp)

target_link_libraries(hle PUBLIC yaml-cpp common)
target_link_libraries(hle PRIVATE coreloader)
target_include_directories(hle PUBLIC ${YAML_INCLUDE_DIR})
target_include_directories(hle PUBLIC  ${CORE_INCLUDE_DIR})