#include <epoc/services/drm/helper.h>

namespace eka2l1 {
    drm_helper_server::drm_helper_server(eka2l1::system *sys) 
        : service::server(sys, "CDRMHelperServer", true) {
    }
}