add_library(coreloader
	${CORE_INCLUDE_DIR}/loader/sis_fields.h
	${CORE_INCLUDE_DIR}/loader/sis.h
	${CORE_INCLUDE_DIR}/loader/sis_old.h
	${CORE_INCLUDE_DIR}/loader/sis_script_interpreter.h
	${CORE_INCLUDE_DIR}/loader/eka2img.h
    ${CORE_INCLUDE_DIR}/loader/romimage.h
    ${CORE_INCLUDE_DIR}/loader/rom.h
    ${CORE_INCLUDE_DIR}/loader/rpkg.h
	eka2img.cpp
	sis.cpp
	sis_fields.cpp
    sis_old.cpp
    sis_script_interpreter.cpp
    romimage.cpp
    rpkg.cpp
	rom.cpp)

target_link_libraries(coreloader 
    PRIVATE
    common 
    coreio
    coremem
    corekernel
    hle
    miniz
    yaml-cpp) 

target_include_directories(coreloader PUBLIC ${CORE_OUTER_INCLUDE_DIR})
target_include_directories(coreloader
    PUBLIC
    ${YAML_INCLUDE_DIR})