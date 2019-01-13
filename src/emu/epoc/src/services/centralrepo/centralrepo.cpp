#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/ini.h>

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
            t = central_repo_entry_type::string;
            return true;
        }

        if (tok == "string") {
            t = central_repo_entry_type::string;
            return true;
        }

        if (tok == "real") {
            t = central_repo_entry_type::real;
            return true;
        }

        return false;
    }

    bool parse_new_centrep_ini(const std::string &path, central_repo &repo) {
        common::ini_file creini;
        int err = creini.load(path.c_str());

        if (err < 0) {
            return false;
        }

        // Get the version
        if (!creini.find("cenrep")) {
            return false;
        }

        common::ini_prop *crerep_ver_info = creini.find("cenrep")->get_as<common::ini_prop>();
        repo.ver = std::atoi(crerep_ver_info->get_value().c_str());

        // Get owner section
        common::ini_section *crerep_owner = creini.find("owner")->get_as<common::ini_section>();
        repo.owner_uid = crerep_owner->get<common::ini_value>(0)->get_as_native<std::uint32_t>();

        // Handle default metadata
        common::ini_section *crerep_default_meta = creini.find("defaultmeta")->get_as<common::ini_section>();
        
        if (crerep_default_meta) {
            for (auto &node: (*crerep_default_meta)) {
                switch (node->get_node_type()) {
                case common::INI_NODE_VALUE: {
                    // There is only a single value
                    // That should be the default metadata for everyone!!!
                    repo.default_meta = node->get_as<common::ini_value>()->get_as_native<std::uint32_t>();
                    break;
                }

                case common::INI_NODE_PAIR: {
                    common::ini_pair *p = node->get_as<common::ini_pair>();
                    central_repo_default_meta def_meta;

                    // Iterate through each elements to determine
                    std::uint32_t potientially_meta = p->key_as<std::uint32_t>();
                    common::ini_node *n = p->get<common::ini_node>(0);

                    if (n && n->get_node_type() == common::INI_NODE_KEY && 
                        strncmp(n->name(), "mask", 4) == 0) {
                        // That's a mask, we should add new
                        def_meta.default_meta_data = potientially_meta;
                        def_meta.key_mask = n->get_as<common::ini_prop>()->get_as_native<std::uint32_t>();
                        def_meta.low_key = p->get<common::ini_value>(1)->get_as_native<std::uint32_t>();
                    } else {
                        // Nope
                        def_meta.low_key = potientially_meta;
                        def_meta.high_key = n->get_as<common::ini_value>()->get_as_native<std::uint32_t>();
                        def_meta.default_meta_data = p->get<common::ini_value>(1)->get_as_native<std::uint32_t>();
                    }

                    repo.meta_range.push_back(def_meta);
                    break;
                }

                default: {
                    LOG_ERROR("Unsupported metadata INI node type");
                    return false;
                }
                }
            }
        }

        // TODO (pent0): Capability supply
        // Section: platsec

        // Main section
        common::ini_section *main = creini.find("Main")->get_as<common::ini_section>();
        // Iterate through each ini to get entry

        if (!main) {
            return false;
        }

        for (auto &node: (*main)) {
            common::ini_pair *p = node->get_as<common::ini_pair>();
            central_repo_entry entry;

            std::string key_num_str = p->name();

            if (key_num_str.length() > 2 && key_num_str[0] == '0' && (key_num_str[1] == 'x' || key_num_str[1] == 'X')) {
                key_num_str = key_num_str.substr(2, key_num_str.length() - 2);
                entry.key = static_cast<std::uint32_t>(std::strtoll(key_num_str.c_str(), nullptr, 16));
            } else {
                entry.key = static_cast<decltype(entry.key)>(std::atoi(key_num_str.c_str()));
            }

            std::string entry_type = p->get<common::ini_value>(0)->get_value();

            if (!indentify_central_repo_entry_var_type(entry_type, entry.data.etype)) {
                return false;
            }

            switch (entry.data.etype) {
            case central_repo_entry_type::integer: {
                entry.data.intd = p->get<common::ini_value>(1)->get_as_native<std::uint64_t>();
                break;
            }

            case central_repo_entry_type::string: {
                entry.data.strd = p->get<common::ini_value>(1)->get_value();
                break;
            }

            case central_repo_entry_type::real: {
                entry.data.reald = p->get<common::ini_value>(1)->get_as_native<double>();
                break;
            }

            default: {
                assert(false && "Unsupported data type!");
                break;
            }
            }

            if (p->get_value_count() >= 3) {
                entry.metadata_val = p->get<common::ini_value>(2)->get_as_native<std::uint32_t>();
                repo.add_new_entry(entry.key, entry.data, entry.metadata_val);
            } else {
                repo.add_new_entry(entry.key, entry.data);
            }

            // TODO (pent0): Capability supply
        }

        return true;
    }
}
