add_library(coreloader
	${CORE_INCLUDE_DIR}/loader/sis_fields.h
	${CORE_INCLUDE_DIR}/loader/sis.h
	${CORE_INCLUDE_DIR}/loader/sis_script_interpreter.h
	${CORE_INCLUDE_DIR}/loader/eka2img.h
	eka2img.cpp
	sis.cpp
	sis_fields.cpp
	sis_script_interpreter.cpp)

target_link_libraries(coreloader PUBLIC common)
target_link_libraries(coreloader PRIVATE coremem miniz core)

target_include_directories(coreloader PUBLIC ${CORE_INCLUDE_DIR})
