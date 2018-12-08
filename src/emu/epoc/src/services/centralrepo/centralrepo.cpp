#include <common/algorithm.h>
#include <common/log.h>
#include <common/cvt.h>

#include <epoc/services/centralrepo/centralrepo.h>

#include <fstream>
#include <sstream>
#include <vector>

namespace eka2l1 {
    // TODO: Security check. This include reading keyspace file (.cre) to get policies
    // information and reading capabilities section

    struct ini_section {
        std::string name;
        std::vector<std::string> data;
    };

    std::uint32_t token_to_number(const std::string &tok) {
        std::string beg = tok.substr(0, 2);

        if (beg == "0x" || beg == "0X") {
            return std::stoull(tok.substr(2), 0, 16);
        }

        return std::stoull(tok.substr(2), 0, 10);
    }
    
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

    bool parse_new_centrep_ini(const std::string &path, central_repo &repo) {
        std::ifstream stream(path);

        if (stream.fail()) {
            return false;
        }

        std::string line;
        std::vector<ini_section> sections;

        while (std::getline(stream, line)) {
            std::istringstream line_stream(line);
            std::string token;

            bool cont = false;

            while (line_stream >> token) {
                if (token == "#" || token == ";") {
                    cont = true;
                    break;
                }

                if (token.length() >= 1 && token[0] == '[' && token.back() == ']') {
                    ini_section section;
                    section.name = token.substr(1, token.length() - 2);

                    sections.push_back(std::move(section));
                    cont = true;

                    break;
                }
            }

            if (!cont) {
                sections.back().data.push_back(line);
                continue;
            }
        }

        bool main_found = false;

        // Now that we have done parsing the ini files, we should lurk for important section
        for (const auto &section: sections) {
            if (section.name == "owner") {
                // There should be the ID right after
                if (section.data.size() == 0) {
                    return false;
                }

                repo.owner = token_to_number(section.data[0]);
            } else if (section.name == "Main") {
                main_found = true;

                // Start processing entry
                for (const auto &et: section.data) {
                    if (et == "") {
                        continue;
                    }

                    std::istringstream token_stream(et);
                    std::string tok;

                    if (!(token_stream >> tok)) {
                        return false;
                    }

                    std::size_t key = 0;

                    try {
                        key = token_to_number(tok);
                    } catch (std::exception &e) {
                        LOG_ERROR("The key is invalid {} (not a number)", tok);
                        return false;
                    }

                    if (!(token_stream >> tok)) {
                        return false;
                    }

                    central_repo_entry &entry = repo.entries[key];
                    entry.key = key;

                    if (!indentify_central_repo_entry_var_type(tok, entry.data.etype)) {
                        LOG_ERROR("Unidentify key type {}", tok);
                        return false;
                    }

                    if (!(token_stream >> tok)) {
                        return false;
                    }

                    // Convert data to specified type
                    switch (entry.data.etype) {
                    case central_repo_entry_type::integer: {
                        try {
                            entry.data.intd = token_to_number(tok);
                        } catch (std::exception &e) {
                            LOG_ERROR("Key {} has type int, but {} is given", key, tok);
                            return false;
                        }

                        break;
                    }

                    case central_repo_entry_type::real: {
                        try {
                            entry.data.reald = std::stof(tok, 0);
                        } catch (std::exception &e) {
                            LOG_ERROR("Key {} has type real, but {} is given", key, tok);
                            return false;
                        }
                    }

                    case central_repo_entry_type::string16:
                    case central_repo_entry_type::string8: {
                        bool s16 = false;
                        
                        if (entry.data.etype == central_repo_entry_type::string16) {
                            s16 = true;
                        }

                        // Not a string
                        if (tok.length() == 1 || (tok[0] != '"') || tok[tok.length() - 1] != '"') {
                            LOG_ERROR("Key {} has type string, but {} doesn't contains enough requirements to become one",
                                key, tok);

                            return false;
                        }

                        if (s16) {
                            entry.data.str16d = common::utf8_to_ucs2(tok.substr(1, tok.length() - 2));
                        } else {
                            entry.data.strd = tok.substr(1, tok.length() - 2);
                        }

                        break;
                    }

                    default: break;
                    }

                    if (!(token_stream >> tok)) {
                        return false;
                    }

                    try {
                        entry.metadata_val = token_to_number(tok);
                    } catch (std::exception &e) {
                        LOG_ERROR("Metadata value for key {} is invalid ({})", key, tok);
                        return false;
                    }
                }
            }
        }
    }
}
