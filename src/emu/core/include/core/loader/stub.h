#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <ptr.h>

namespace eka2l1 {
    namespace loader {
        using sid = uint32_t;

        class eka2img;
        class romimg;

        // A stub contains a Symbol ID and a description and
        // where the stub reside in memory
        struct stub {
            sid id;
            std::string des;
            ptr<void> pos;

            virtual void write_stub();
        };

        // A function stub. Basiclly same as normal stub
        struct function_stub : public stub {
            void write_stub() override;
        };

        // A typeinfo stub point to the memory which has the name
        // of the class.
        struct typeinfo_stub : public stub {
            std::string info_des;

            void write_stub() override;
        };

        // Vtable (8 bytes), contains pointer to functions for Inheritance
        // and Polymorphysm.
        struct vtable_stub : public stub {
            stub *mini_stubs;

            void add_stub(stub &st);
            void remove_stub();

            void write_stub() override;
        };

        // A class stub. Contains function stubs, typeinfo stub and a vtable!
        struct class_stub : public stub {
            std::vector<function_stub> fn_stubs;
            typeinfo_stub typeinf_stub;
            vtable_stub vtab_stub;

            uint32_t crr_stub;

            function_stub *new_function_stub() {
                fn_stubs.push_back(function_stub{});
                return &fn_stubs.back();
            }

            stub *get_next_stub();
            void write_stub() override;
        };

        // A module stub contains multiple function stubs and class stubs
        struct module_stub : public stub {
            std::vector<function_stub> pub_func_stubs;
            std::vector<class_stub> class_stubs;

            function_stub *new_function_stub() {
                pub_func_stubs.push_back(function_stub{});
                return &pub_func_stubs.back();
            }

            class_stub *new_class_stub() {
                class_stubs.push_back(class_stub{});
                return &class_stubs.back();
            }

            stub *get_next_stub();
            void write_stub() override;
        };

        // Extract the library function from the database, then write the stub into memory
        module_stub write_module_stub(std::string name, const std::string &db_path = "");
    }
}
