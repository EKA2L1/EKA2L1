#include <epoc/services/akn/icon.h>

namespace eka2l1 {
    akn_icon_server::akn_icon_server(eka2l1::system *sys)
        : service::typical_server(sys, "!AknIconServer") {
    }
}
