if (ENABLE_SCRIPTING_ABILITY)
	set(SCRIPT_MANAGER_FILES
    ${CORE_INCLUDE_DIR}/manager/script_manager.h
	script_manager.cpp)
endif()

add_library(coremanager
	${CORE_INCLUDE_DIR}/manager/manager.h
	${CORE_INCLUDE_DIR}/manager/package_manager.h
	manager.cpp
	package_manager.cpp
	${SCRIPT_MANAGER_FILES})

target_include_directories(coremanager PUBLIC ${CORE_OUTER_INCLUDE_DIR})

# Python libraries should be shared since i don't want passing library around and
# it will be used very common
target_link_libraries(coremanager PUBLIC common)

if (ENABLE_SCRIPTING_ABILITY)
	target_link_libraries(coremanager PUBLIC pybind11::embed)
	target_link_libraries(coremanager PRIVATE symemu)
endif (ENABLE_SCRIPTING_ABILITY)

target_link_libraries(coremanager PRIVATE coreloader)