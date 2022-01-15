namespace eka2l1::arm::r12l1 {
    void register_imb_range_test();
    void register_reg_cache_stress_test();

    void register_all_tests() {
        register_imb_range_test();
        register_reg_cache_stress_test();
    }
}