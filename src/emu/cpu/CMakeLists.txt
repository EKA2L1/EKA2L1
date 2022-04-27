set(SOURCE_12L1R_PUBLIC
        include/cpu/12l1r/common.h
        include/cpu/12l1r/exclusive_monitor.h
        include/cpu/12l1r/tlb.h
        src/12l1r/common.cpp
        src/12l1r/exclusive_monitor.cpp)

set(SOURCE_12L1R
        include/cpu/12l1r/decoder/decoder_detail.h
        include/cpu/12l1r/decoder/matcher.h
        include/cpu/12l1r/encoding/arm.h
        include/cpu/12l1r/encoding/thumb16.h
        include/cpu/12l1r/encoding/thumb32.h
        include/cpu/12l1r/arm_12l1r.h
        include/cpu/12l1r/arm_visitor.h
        include/cpu/12l1r/block_cache.h
        include/cpu/12l1r/block_gen.h
        include/cpu/12l1r/core_state.h
        include/cpu/12l1r/float_marker.h
        include/cpu/12l1r/reg_cache.h
        include/cpu/12l1r/reg_loc.h
        include/cpu/12l1r/thumb_visitor.h
        include/cpu/12l1r/visit_session.h
        src/12l1r/tests/imb_range.cpp
        src/12l1r/tests/reg_cache_stress.cpp
        src/12l1r/tests/test_cpu.cpp
        src/12l1r/tests/test_cpu.h
        src/12l1r/tests/test_entry.cpp
        src/12l1r/translate/branch.cpp
        src/12l1r/translate/coprocessor.cpp
        src/12l1r/translate/data_processing.cpp
        src/12l1r/translate/extension.cpp
        src/12l1r/translate/hint.cpp
        src/12l1r/translate/interrupt.cpp
        src/12l1r/translate/load_store.cpp
        src/12l1r/translate/miscs.cpp
        src/12l1r/translate/multiply.cpp
        src/12l1r/translate/saturated.cpp
        src/12l1r/translate/status_register_access.cpp
        src/12l1r/translate/synchronization.cpp
        src/12l1r/translate/vfp.cpp
        src/12l1r/arm_12l1r.cpp
        src/12l1r/arm_visitor.cpp
        src/12l1r/block_cache.cpp
        src/12l1r/block_gen.cpp
        src/12l1r/core_state.cpp
        src/12l1r/float_marker.cpp
        src/12l1r/reg_cache.cpp
        src/12l1r/reg_loc.cpp
        src/12l1r/thumb_visitor.cpp
        src/12l1r/visit_session.cpp)

set(SOURCE_DYNCOM
        include/cpu/dyncom/vfp/asm_vfp.h
        include/cpu/dyncom/vfp/vfp_helper.h
        include/cpu/dyncom/vfp/vfp.h
        include/cpu/dyncom/vfp/vfpinstr.h
        include/cpu/dyncom/arm_dyncom.h
        include/cpu/dyncom/arm_dyncom_dec.h
        include/cpu/dyncom/arm_dyncom_interpreter.h
        include/cpu/dyncom/arm_dyncom_run.h
        include/cpu/dyncom/arm_dyncom_thumb.h
        include/cpu/dyncom/arm_dyncom_trans.h
        include/cpu/dyncom/arm_regformat.h
        include/cpu/dyncom/armstate.h
        include/cpu/dyncom/armsupp.h
        src/dyncom/vfp/vfp.cpp
        src/dyncom/vfp/vfpdouble.cpp
        src/dyncom/vfp/vfpsingle.cpp
        src/dyncom/arm_dyncom.cpp
        src/dyncom/arm_dyncom_dec.cpp
        src/dyncom/arm_dyncom_interpreter.cpp
        src/dyncom/arm_dyncom_thumb.cpp
        src/dyncom/arm_dyncom_trans.cpp
        src/dyncom/armstate.cpp
        src/dyncom/armsupp.cpp)

add_library(cpu
        include/cpu/arm_analyser.h
        include/cpu/arm_analyser_capstone.h
        include/cpu/arm_factory.h
        include/cpu/arm_interface.h
        include/cpu/arm_utils.h
        src/arm_analyser_capstone.cpp
        src/arm_analyser.cpp
        src/arm_factory.cpp
        src/arm_utils.cpp
        ${SOURCE_12L1R_PUBLIC}
        ${SOURCE_DYNCOM})

if (ARCHITECTURE_ARM32)
    target_sources(cpu PRIVATE ${SOURCE_12L1R})
    target_link_libraries(cpu PRIVATE Catch2)
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