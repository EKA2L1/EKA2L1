#pragma once

#define ADD_SVC_REGISTERS(mngr, map) mngr.svc_funcs.insert(map.begin(), map.end())

namespace eka2l1::hle {
    class lib_manager;
}

namespace eka2l1::epoc {
    void register_epocv93(hle::lib_manager &mngr);
    void register_epocv94(hle::lib_manager &mngr);
}