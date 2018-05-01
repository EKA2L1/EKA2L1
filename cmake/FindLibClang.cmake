find_path(libclang_include_dir clang-c/Index.h)
find_library(libclang_lib NAMES clang)

set(LIBCLANG_LIBRARIES ${libclang_lib})
set(LIBCLANG_INCLUDE_DIRS ${libclang_include_dir})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libclang DEFAULT_MSG libclang_lib libclang_include_dir)

mark_as_advanced(libclang_include_dir libclang_lib)
