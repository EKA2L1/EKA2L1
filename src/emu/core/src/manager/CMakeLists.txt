add_library(coremanager
	${CORE_INCLUDE_DIR}/manager/manager.h
	${CORE_INCLUDE_DIR}/manager/package_manager.h
    ${CORE_INCLUDE_DIR}/manager/script_manager.h
    script_manager.cpp
	manager.cpp
	package_manager.cpp)

target_include_directories(coremanager PUBLIC ${CORE_OUTER_INCLUDE_DIR})

# Python libraries should be shared since i don't want passing library around and
# it will be used very common
target_link_libraries(coremanager PUBLIC common pybind11::embed)
target_link_libraries(coremanager PRIVATE coreloader symemu)
	
