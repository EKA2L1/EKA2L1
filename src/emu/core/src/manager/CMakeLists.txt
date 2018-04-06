add_library(coremanager
	${CORE_INCLUDE_DIR}/manager/manager.h
	${CORE_INCLUDE_DIR}/manager/package_manager.h
	manager.cpp
	package_manager.cpp)

target_include_directories(coremanager PUBLIC ${CORE_INCLUDE_DIR})
target_link_libraries(coremanager PUBLIC common)
target_link_libraries(coremanager PRIVATE coreloader)
	
