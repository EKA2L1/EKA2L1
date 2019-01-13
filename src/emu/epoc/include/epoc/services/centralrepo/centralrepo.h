#pragma once

#include <epoc/services/centralrepo/repo.h>
#include <epoc/services/server.h>

#include <map>
#include <string>
#include <unordered_map>

#define CENTRAL_REPO_UID_STRING "10202be9"

namespace eka2l1 {
    /*! \brief Parse a new centrep ini file.
	 *
	 * \returns False if IO error or invalid centrep configs.
	*/
    bool parse_new_centrep_ini(const std::string &path, central_repo &repo);
    
    struct central_repo_client_session {
        central_repo *attach_repo;
        central_repo_transaction_mode lock_mode;
    };

    class central_repo_server : public service::server {
        // Cached repos. The key is the owner of the repo.
        std::unordered_map<std::uint32_t, central_repo> repos;
        std::map<std::uint32_t, central_repo_client_session> client_sessions;

    public:
        explicit central_repo_server(eka2l1::system *sys);

        void init(service::ipc_context ctx);

        void create_int(service::ipc_context ctx);
        void create_real(service::ipc_context ctx);
        void create_string8(service::ipc_context ctx);
        void create_string16(service::ipc_context ctx);

        void transaction_start(service::ipc_context ctx);
        void transaction_commit(service::ipc_context ctx);
    };
}