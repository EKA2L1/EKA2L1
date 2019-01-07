#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/log.h>

#include <epoc/services/centralrepo/centralrepo.h>

#include <fstream>
#include <sstream>
#include <vector>

namespace eka2l1 {
    // TODO: Security check. This include reading keyspace file (.cre) to get policies
    // information and reading capabilities section
    bool indentify_central_repo_entry_var_type(const std::string &tok, central_repo_entry_type &t) {
        if (tok == "int") {
            t = central_repo_entry_type::integer;
            return true;
        }

        if (tok == "string8") {
            t = central_repo_entry_type::string8;
            return true;
        }

        if (tok == "string") {
            t = central_repo_entry_type::string16;
            return true;
        }

        if (tok == "real") {
            t = central_repo_entry_type::real;
            return true;
        }

        return false;
    }
}
