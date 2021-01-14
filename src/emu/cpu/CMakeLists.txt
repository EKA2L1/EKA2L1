set(SOURCE_12L1R
        include/cpu/12l1r/decoder/decoder_detail.h
        include/cpu/12l1r/decoder/matcher.h
        include/cpu/12l1r/encoding/arm.h
        include/cpu/12l1r/encoding/thumb16.h
        include/cpu/12l1r/arm_12l1r.h
        include/cpu/12l1r/arm_visitor.h
        include/cpu/12l1r/block_cache.h
        include/cpu/12l1r/block_gen.h
        include/cpu/12l1r/common.h
        include/cpu/12l1r/core_state.h
        include/cpu/12l1r/reg_cache.h
        include/cpu/12l1r/reg_loc.h
        include/cpu/12l1r/thumb_visitor.h
        include/cpu/12l1r/tlb.h
        include/cpu/12l1r/visit_session.h
        src/12l1r/translate/branch.cpp
        src/12l1r/translate/data_processing.cpp
        src/12l1r/translate/interrupt.cpp
        src/12l1r/translate/load_store.cpp
        src/12l1r/translate/multiply.cpp
        src/12l1r/translate/status_register_access.cpp
        src/12l1r/arm_12l1r.cpp
        src/12l1r/arm_visitor.cpp
        src/12l1r/block_cache.cpp
        src/12l1r/block_gen.cpp
        src/12l1r/common.cpp
        src/12l1r/core_state.cpp
        src/12l1r/reg_cache.cpp
        src/12l1r/reg_loc.cpp
        src/12l1r/thumb_visitor.cpp
        src/12l1r/visit_session.cpp)

add_library(cpu
        include/cpu/arm_analyser.h
        include/cpu/arm_analyser_capstone.h
        include/cpu/arm_factory.h
        include/cpu/arm_interface.h
        include/cpu/arm_utils.h
        src/arm_analyser_capstone.cpp
        src/arm_analyser.cpp
        src/arm_factory.cpp
        src/arm_utils.cpp)

if (ARCHITECTURE_ARM32)
    target_sources(cpu PRIVATE
            ${SOURCE_12L1R})
else()
    target_sources(cpu PRIVATE
            include/cpu/arm_dynarmic.h
            src/arm_dynarmic.cpp)
    target_link_libraries(cpu
            PRIVATE
            dynarmic)
endif()

target_include_directories(cpu PUBLIC include)
target_include_directories(cpu PRIVATE ${capstone_INCLUDE_DIRS})

target_link_libraries(cpu PUBLIC common)
target_link_libraries(cpu
        PRIVATE
        capstone-static)