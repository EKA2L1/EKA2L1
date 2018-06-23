#include <services/featmgr/featmgr.h>

namespace eka2l1 {
    featmgr_server::featmgr_server(system *sys)
        : service::server(sys, "!FeatMgrServer") {

    }
}