macro(add_assets DETARGET PATH DESTPATH)
    add_custom_command(
                TARGET ${DETARGET}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${PATH}" "$<TARGET_FILE_DIR:${DETARGET}>/${DESTPATH}/")
endmacro()
