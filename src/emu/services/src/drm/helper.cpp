#include <services/drm/helper.h>
#include <epoc/epoc.h>

namespace eka2l1 {
    drm_helper_server::drm_helper_server(eka2l1::system *sys)
        : service::server(sys->get_kernel_system(), sys, "CDRMHelperServer", true) {
    }
}