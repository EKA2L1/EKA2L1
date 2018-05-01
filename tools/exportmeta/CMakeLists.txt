add_executable(exportmeta src/main.cpp)
target_link_libraries(exportmeta PRIVATE
     common libclang)
