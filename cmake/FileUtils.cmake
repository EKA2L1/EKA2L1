macro(add_assets DETARGET PATH DESTPATH)
    file(GLOB assets LIST_DIRECTORIES TRUE RELATIVE ${PATH} ${PATH}/*)

    add_custom_command(
                TARGET ${DETARGET}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${DETARGET}>/${DESTPATH}/")

    foreach (asset ${assets})
        if (IS_DIRECTORY ${PATH}/${asset})
            add_assets(${DETARGET} ${PATH}/${asset} ${DESTPATH}/${asset})
        else()
            add_custom_command(
                    TARGET ${DETARGET}
                    POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy "${PATH}/${asset}" "$<TARGET_FILE_DIR:${DETARGET}>/${DESTPATH}/")
        endif()
    endforeach()
endmacro()
