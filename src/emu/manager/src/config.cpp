#include <manager/config.h>
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace eka2l1::manager {
    template <typename T, typename Q = T>
    void get_yaml_value(YAML::Node &config_node, const char *key, T *target_val, Q default_val) {
        try {
            *target_val = config_node[key].as<T>();
        } catch (...) {
            *target_val = std::move(default_val);
        }
    }
    
    template <typename T>
    void config_file_emit_single(YAML::Emitter &emitter, const char *name, T &val) {
        emitter << YAML::Key << name << YAML::Value << val;
    }

    template <typename T>
    void config_file_emit_vector(YAML::Emitter &emitter, const char *name, std::vector<T> &values) {
        emitter << YAML::Key << name << YAML::BeginSeq;

        for (const T &value : values) {
            emitter << value;
        }

        emitter << YAML::EndSeq;
    }

    void config_state::serialize() {
        YAML::Emitter emitter;
        emitter << YAML::BeginMap;

        config_file_emit_single(emitter, "bkg-alpha", bkg_transparency);
        config_file_emit_single(emitter, "bkg-path", bkg_path);
        config_file_emit_single(emitter, "font", font_path);
        config_file_emit_single(emitter, "log-read", log_read);
        config_file_emit_single(emitter, "log-write", log_write);
        config_file_emit_single(emitter, "log-ipc", log_ipc);
        config_file_emit_single(emitter, "log-svc", log_svc);
        config_file_emit_single(emitter, "log-passed", log_passed);
        config_file_emit_single(emitter, "log-exports", log_exports);
        config_file_emit_single(emitter, "log-code", log_code);
        config_file_emit_single(emitter, "enable-breakpoint-script", enable_breakpoint_script);
        config_file_emit_vector(emitter, "force-load", force_load_modules);
        config_file_emit_single(emitter, "cpu", cpu_backend);
        config_file_emit_single(emitter, "device", device);
        config_file_emit_single(emitter, "enable-gdb-stub", enable_gdbstub);
        config_file_emit_single(emitter, "rom", rom_path);
        config_file_emit_single(emitter, "c-mount", c_mount);
        config_file_emit_single(emitter, "e-mount", e_mount);
        config_file_emit_single(emitter, "z-mount", z_mount);
        config_file_emit_single(emitter, "gdb-port", gdb_port);

        emitter << YAML::EndMap;
        
        std::ofstream file("config.yml");
        file << emitter.c_str();
    }

    void config_state::deserialize() {
        YAML::Node node;

        try {
            node = YAML::LoadFile("config.yml");
        } catch (...) {
            serialize();
            return;
        }

        get_yaml_value(node, "bkg-alpha", &bkg_transparency, 129);
        get_yaml_value(node, "bkg-path", &bkg_path, "");
        get_yaml_value(node, "font", &font_path, "");
        get_yaml_value(node, "log-read", &log_read, false);
        get_yaml_value(node, "log-write", &log_write, false);
        get_yaml_value(node, "log-ipc", &log_ipc, false);
        get_yaml_value(node, "log-svc", &log_svc, false);
        get_yaml_value(node, "log-passed", &log_passed, false);
        get_yaml_value(node, "log-exports", &log_exports, false);
        get_yaml_value(node, "log-code", &log_code, false);
        get_yaml_value(node, "enable-breakpoint-script", &enable_breakpoint_script, false);
        get_yaml_value(node, "cpu", &cpu_backend, 0);
        get_yaml_value(node, "device", &device, 0);
        get_yaml_value(node, "enable-gdb-stub", &enable_gdbstub, false);
        get_yaml_value(node, "rom", &rom_path, "SYM.ROM");
        get_yaml_value(node, "c-mount", &c_mount, "drives/c/");
        get_yaml_value(node, "e-mount", &e_mount, "drives/e/");
        get_yaml_value(node, "z-mount", &z_mount, "drives/z/");
        get_yaml_value(node, "gdb-port", &gdb_port, 24689);

        try {
            YAML::Node force_loads_node = node["force-load"];

            for (auto force_load_node : force_loads_node) {
                force_load_modules.push_back(force_load_node.as<std::string>());
            }
        } catch (...) {
        }
    }
}
