#include <services/fontbitmap/fontbitmap.h>

namespace eka2l1 {
    fontbitmap_server::fontbitmap_server(system *sys)
        : service::server(sys, "!Fontbitmapserver") {

    }
}