#pragma once

#include <epoc/services/fbs/fbs.h>
#include <epoc/utils/obj.h>

#include <cstdint>
#include <cstddef>
#include <memory>
#include <unordered_map>

namespace eka2l1::service {
    using uid = std::uint32_t;

    class normal_object_container: public epoc::object_container {        
        using ref_count_object_heap_ptr = std::unique_ptr<epoc::ref_count_object>;
        std::vector<ref_count_object_heap_ptr> objs;

        std::atomic<uid> uid_counter;

    public:
        template <typename T, typename ...Args>
        T *make_new(Args... arguments) {
            ref_count_object_heap_ptr obj = std::make_unique<T>(arguments...);
            obj->id = uid_counter++;

            objs.push_back(std::move(obj));

            return objs.back().get();
        }

        bool remove(epoc::ref_count_object *obj) override;
    };

    class typical_session;
    using typical_session_ptr = std::unique_ptr<typical_session>;

    class typical_server: public server {
        friend class typical_session;
        std::unordered_map<service::uid, typical_session_ptr> sessions;
        normal_object_container obj_con;

    public:
        explicit typical_server(system *sys, const std::string name);
        void process_accepted_msg() override;
    };
    
    class typical_session {
        typical_server *svr_;

    protected:
        service::uid client_ss_uid;
        epoc::object_table obj_table;

    public:
        template <typename T, typename ...Args>
        T *make_new(Args... arguments) {
            return svr_->obj_con.make_new<T, Args...>(Args...);
        }

        virtual void fetch(service::ipc_context &ctx) = 0;
    };
}
