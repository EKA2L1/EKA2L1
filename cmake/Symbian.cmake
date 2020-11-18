# Add a Symbian project for building in release mode
function(add_symbian_project FOLDER)
    add_custom_target(${FOLDER})
    add_custom_command(
        TARGET ${FOLDER}
        COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/patch_build.py ${CMAKE_CURRENT_SOURCE_DIR}/${FOLDER}/group)

    get_property(LAST_TARGET GLOBAL PROPERTY LAST_SYMBIAN_PROJECT_TARGET)

    if (LAST_TARGET)
        add_dependencies(${FOLDER} ${LAST_TARGET})
    endif()

    set_property(GLOBAL PROPERTY LAST_SYMBIAN_PROJECT_TARGET "${FOLDER}")
endfunction()

function(add_symbian_patch NAME)
    add_symbian_project(${NAME})

    set_target_properties(${NAME} PROPERTIES OUTPUT_NAME ${NAME}
	    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/patch"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/patch")

    add_custom_command(
	    TARGET ${NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/${NAME}/group" "${CMAKE_BINARY_DIR}/bin/patch/")
endfunction()