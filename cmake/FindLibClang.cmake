if (UNIX)
    set (libclang_search
        /usr/lib/llvm-3.8/include/
        /usr/lib/llvm-4.0/include/
        /usr/lib/llvm-5.0/include/
        /usr/local/llvm/include/
        /usr/lib/llvm-3.8/lib/
        /usr/lib/llvm-4.0/lib/
        /usr/lib/llvm-5.0/lib/
        /usr/local/llvm/lib/
        /opt/llvm/include/
        /opt/llvm/lib/)
elseif (WIN32)
    set (libclang_search
        $ENV{PATH}
        $ENV{INCLUDE}
        $ENV{LIB}
        C:\\Program Files\\LLVM\\include\\
        C:\\Program Files\\LLVM\\lib\\)
endif()

find_path(libclang_include_dir NAMES clang-c/Index.h PATHS ${libclang_search})
find_library(libclang_lib NAMES clang PATHS ${libclang_search})

set(LIBCLANG_LIBRARIES ${libclang_lib})
set(LIBCLANG_INCLUDE_DIRS ${libclang_include_dir})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libclang DEFAULT_MSG libclang_lib libclang_include_dir)

mark_as_advanced(libclang_lib libclang_include_dir)
