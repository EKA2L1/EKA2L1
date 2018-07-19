#include <core/epoc/reg.h>
#include <core/epoc/svc.h>

#include <core/hle/bridge.h>
#include <core/hle/libmanager.h>

namespace eka2l1::epoc {
    void register_epocv93(eka2l1::hle::lib_manager &mngr) {
        ADD_SVC_REGISTERS(mngr, svc_register_funcs_v93);
    }

    void register_epocv94(eka2l1::hle::lib_manager &mngr) {
        ADD_SVC_REGISTERS(mngr, svc_register_funcs_v94);
    }
}