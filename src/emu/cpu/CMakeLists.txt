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
        src/12l1r/tests/mem_stress.cpp
        src/12l1r/tests/reg_cache_stress.cpp
        src/12l1r/tests/test_cpu.cpp
        src/12l1r/tests/test_cpu.h
        src/12l1r/tests/test_entry.cpp
        src/12l1r/tests/vfp.cpp
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

# Build the CPU module with the dyncom interpreter only (no dynarmic). Used by
# the dyncom differential test harness, which only needs the interpreter and
# otherwise would drag in dynarmic (which fails to build host-side against newer
# libc++ via boost's removed std::unary_function). Default OFF.
option(EKA2L1_CPU_DYNCOM_ONLY "Build the CPU module with the dyncom interpreter only (no dynarmic)" OFF)

# Instrument the dyncom interpreter to log guest opcode / instruction-pair /
# block-length frequencies (used to target super-instruction fusion). Opt-in,
# default OFF -> zero overhead in normal builds.
option(EKA2L1_DYNCOM_PROFILE "Profile guest instruction/pair frequencies in the dyncom interpreter" OFF)

# VFP's instruction-level trace sites sit directly on dyncom's hottest path: one
# log line per float loaded or stored, each synchronously flushed (spdlog
# flush_on = debug). Float-heavy guest code drowns in them. Compile them out
# unless a diagnostic build explicitly asks for them — including in Debug, whose
# default log filter is "*:trace" and would otherwise enable them by default.
option(EKA2L1_DYNCOM_VFP_TRACE "Keep dyncom VFP trace logging" OFF)
if (NOT EKA2L1_DYNCOM_VFP_TRACE)
    target_compile_definitions(cpu PRIVATE EKA2L1_DYNCOM_DISABLE_VFP_TRACE=1)
endif()

if (ARCHITECTURE_ARM32)
    target_sources(cpu PRIVATE ${SOURCE_12L1R})
    target_link_libraries(cpu PRIVATE Catch2)
elseif ((EKA2L1_IOS AND NOT EKA2L1_IOS_DYNARMIC) OR EKA2L1_CPU_DYNCOM_ONLY)
    # iOS builds without EKA2L1_IOS_DYNARMIC run the dyncom interpreter only.
    # This is the App Store / TestFlight configuration: no JIT code is
    # compiled in.
    message(STATUS "cpu: linking without dynarmic (dyncom only)")
    if (EKA2L1_CPU_DYNCOM_ONLY)
        # On a non-iOS host the dynarmic-detection macro would otherwise pull in
        # the dynarmic headers; force it off for the dyncom-only build.
        target_compile_definitions(cpu PRIVATE EKA2L1_CPU_DYNCOM_ONLY_BUILD=1)
    endif()
else()
    target_sources(cpu PRIVATE
            include/cpu/arm_dynarmic.h
            src/arm_dynarmic.cpp)
    target_link_libraries(cpu
            PRIVATE
            dynarmic)
endif()

if (EKA2L1_DYNCOM_PROFILE)
    # PUBLIC so the ARMul_State layout (which gains two profiling fields) is
    # consistent in every TU that includes armstate.h, avoiding an ODR mismatch.
    target_compile_definitions(cpu PUBLIC EKA2L1_DYNCOM_PROFILE=1)
endif()

target_include_directories(cpu PUBLIC include)
target_include_directories(cpu PRIVATE ${capstone_INCLUDE_DIRS})

target_link_libraries(cpu PUBLIC common)
target_link_libraries(cpu
        PRIVATE
        capstone)

# Differential test harness for the dyncom interpreter (self-A/B + golden ALU
# oracle over randomized instruction streams). Standalone host tool, default OFF;
# see scripts/cpu_difftest.sh.
option(EKA2L1_BUILD_DYNCOM_DIFFTEST "Build the dyncom interpreter differential test harness" OFF)
if (EKA2L1_BUILD_DYNCOM_DIFFTEST)
    # The test-only VFP selector lets one process compare host-fast and
    # reference-softfloat execution without changing production state.
    target_compile_definitions(cpu PUBLIC EKA2L1_DYNCOM_DIFFTEST=1)
    add_executable(dyncom_difftest src/dyncom/tests/dyncom_difftest.cpp)
    target_link_libraries(dyncom_difftest PRIVATE cpu common dynarmic)

endif()
