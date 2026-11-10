add_library(control
    src/api.cpp
    src/server.cpp
    src/service.cpp
    src/storage.cpp)
target_include_directories(control PUBLIC include)
target_include_directories(control SYSTEM PRIVATE
    "${PROJECT_SOURCE_DIR}/src/external/ext-boost"
    "${PROJECT_SOURCE_DIR}/src/external/rapidjson/include")
target_compile_definitions(control PRIVATE BOOST_ERROR_CODE_HEADER_ONLY BOOST_ALL_NO_LIB)
set_target_properties(control PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_STANDARD 17)
if(WIN32)
    target_link_libraries(control PRIVATE ws2_32 mswsock)
elseif(NOT ANDROID)
    target_link_libraries(control PRIVATE pthread)
endif()

set(NGAGE_AGENT "${CMAKE_CURRENT_SOURCE_DIR}/guest/ngage/eka2l1_ngage_launch.exe")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${NGAGE_AGENT}")
file(READ "${NGAGE_AGENT}" NGAGE_AGENT_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," NGAGE_AGENT_BYTES "${NGAGE_AGENT_HEX}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ngage_agent.h"
    "static const unsigned char ngage_agent[] = {${NGAGE_AGENT_BYTES}};\n")
add_library(control_guest src/ngage.cpp)
target_include_directories(control_guest PUBLIC include PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(control_guest PUBLIC control PRIVATE epoc epockern epocservs common)
set_target_properties(control_guest PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_STANDARD 17)

if(NOT EKA2L1_IOS)
    add_library(control_native src/native.cpp)
    target_include_directories(control_native PUBLIC include)
    target_link_libraries(control_native PUBLIC control PRIVATE control_guest epoc epockern epocservs epocpkg drivers common zlibstatic)
    set_target_properties(control_native PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
