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

if(NOT EKA2L1_IOS)
    add_library(control_native src/native.cpp)
    target_include_directories(control_native PUBLIC include)
    target_link_libraries(control_native PUBLIC control PRIVATE epoc epockern epocservs epocpkg drivers common zlibstatic)
    set_target_properties(control_native PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
