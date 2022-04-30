namespace eka2l1::arm::r12l1 {
    void register_imb_range_test();
    void register_reg_cache_stress_test();
    void register_vfp_tests();
    void register_mem_stress_tests();

    void register_all_tests() {
        register_imb_range_test();
        register_reg_cache_stress_test();
        register_vfp_tests();
        register_mem_stress_tests();
    }
}