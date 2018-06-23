#include <services/fs/fs.h>

namespace eka2l1 {
    fs_server::fs_server(system *sys)
        : service::server(sys, "!FileServer") {

    }
}