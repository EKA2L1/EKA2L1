#include <epoc/epoc.h>
#include <epoc/kernel/codeseg.h>
#include <epoc/kernel/libmanager.h>
#include <epoc/services/ecom/ecom.h>
#include <epoc/services/ecom/hleutils.h>
#include <epoc/services/fs/std.h>

namespace eka2l1::epoc {
    service::faker::chain *get_implementation_proxy_table(service::faker *pr, eka2l1::ecom_server *serv,
        const std::uint32_t impl_uid) {
        epoc::fs::entry dll_entry;
        epoc::uid dtor_key = 0;

        std::int32_t err_code = 0;

        if (serv->get_implementation_dll_info(pr->main_thread(), 0, impl_uid, dll_entry, dtor_key, &err_code, false)) {
            return nullptr;
        }

        // Get the entry of the implementation library
        codeseg_ptr seg = serv->get_system()->get_lib_manager()->load(dll_entry.name.to_std_string(pr->process()),
            pr->process());

        if (!seg) {
            return nullptr;
        }

        const address proxy_inst_method = seg->lookup(pr->process(), 1);
        return pr->then(proxy_inst_method, pr->new_temp_arg<std::int32_t>());
    }
}