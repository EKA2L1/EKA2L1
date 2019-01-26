if (ENABLE_SCRIPTING_ABILITY)
	set(SCRIPT_MANAGER_FILES
    include/manager/script_manager.h
	src/script_manager.cpp)
endif()

add_library(manager
	include/manager/manager.h
	include/manager/package_manager.h
	include/manager/sis_common.h
	include/manager/sis_fields.h
	include/manager/sis_old.h
	include/manager/sis_script_interpreter.h
	include/manager/sis_v1_installer.h
	include/manager/sis.h
	src/manager.cpp
	src/package_manager.cpp
	src/sis_fields.cpp
	src/sis_old.cpp
	src/sis_script_interpreter.cpp
	src/sis_v1_installer.cpp
	src/sis.cpp
	${SCRIPT_MANAGER_FILES})

target_include_directories(manager PUBLIC include)

# Python libraries should be shared since i don't want passing library around and
# it will be used very common
target_link_libraries(manager PUBLIC common)

if (ENABLE_SCRIPTING_ABILITY)
	target_link_libraries(manager PUBLIC pybind11::embed)
	target_link_libraries(manager PRIVATE symemu epockern)

	if (NOT MSVC)
		target_link_libraries(manager PRIVATE stdc++fs)
	endif()
endif (ENABLE_SCRIPTING_ABILITY)

target_link_libraries(manager PRIVATE epocio miniz yaml-cpp)