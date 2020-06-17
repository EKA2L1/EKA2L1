#pragma once

#define ADD_SVC_REGISTERS(mngr, map) mngr.svc_funcs_.insert(map.begin(), map.end())

namespace eka2l1::hle {
    class lib_manager;
}

namespace eka2l1::epoc {
    /**
     * @brief Register Symbian 9.3 supervisor calls.
     * @param mngr Reference to library manager.
     **/
    void register_epocv93(hle::lib_manager &mngr);

    /**
     * @brief Register Symbian 9.4 supervisor calls.
     * @param mngr Reference to library manager.
     **/
    void register_epocv94(hle::lib_manager &mngr);

    /**
     * @brief Register Symbian S^3 supervisor calls.
     * @param mngr Reference to library manager.
     **/
    void register_epocv10(eka2l1::hle::lib_manager &mngr);

    /**
     * @brief Register Symbian 6.0 supervisor calls.
     * @param mngr Reference to library manager.
     **/
    void register_epocv6(eka2l1::hle::lib_manager &mngr);
}