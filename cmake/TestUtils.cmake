macro(add_test_assets DETARGET PATH DESTPATH)
    file(GLOB assets RELATIVE ${PATH} ${PATH}/*)

    add_custom_command(
                TARGET ${DETARGET}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/${DESTPATH}/")

    foreach (asset ${assets})
        add_custom_command(
                TARGET ${DETARGET}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PATH}/${asset}" "${CMAKE_CURRENT_BINARY_DIR}/${DESTPATH}/")
    endforeach()
endmacro()
