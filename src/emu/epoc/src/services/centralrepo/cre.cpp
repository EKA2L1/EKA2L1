/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/log.h>

#include <epoc/services/centralrepo/cre.h>
#include <epoc/utils/des.h>
#include <epoc/vfs.h>

#include <manager/device_manager.h>

namespace eka2l1 {
    /* 
     * The header of a CRE file is following:
     *
     * |    Offset           |     Size      |      Description         |     Note
     * |      0              |       1       |      Version             |    
     * |      1              |       4       |    CRE repo UID          |
     * |      5(a)           |       1(a)    |    Keyspace type         |   This extra byte exist if version >= 3
     * |      5              |       4       |    Owner UID             |
     * |      9              |       4       |    Entry count           |
     * 
    */
    int do_state_for_cre(common::chunkyseri &seri, eka2l1::central_repo &repo) {
        std::uint32_t uid1 = 0x10000037; // Direct file store UID
        std::uint32_t uid2 = 0;
        std::uint32_t uid3 = 0x10202BE9; // Cenrep Server UID

        seri.absorb(uid1);
        seri.absorb(uid2);
        seri.absorb(uid3);

        if (uid1 != 0x10000037 || uid3 != 0x10202BE9) {
            return -1;
        }

        // TODO: Figure out usage
        std::uint32_t unk1 = 0xCC776985;
        std::uint32_t unk2 = 0x14;

        seri.absorb(unk1);
        seri.absorb(unk2);

        seri.absorb(repo.ver);
        seri.absorb(repo.uid);

        if (repo.ver >= cenrep_pma_ver) {
            seri.absorb(repo.keyspace_type);
        }

        seri.absorb(repo.owner_uid);

        std::uint32_t single_policies_count = static_cast<std::uint32_t>(repo.single_policies.size());
        seri.absorb(single_policies_count);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            repo.single_policies.resize(single_policies_count);
        }

        auto state_for_a_policy = [&](central_repo_entry_access_policy &policy) {
            seri.absorb(policy.low_key);
            seri.absorb(policy.high_key);
            seri.absorb(policy.key_mask);

            epoc::absorb_des(&policy.read_access, seri);
            epoc::absorb_des(&policy.write_access, seri);
        };

        for (auto &pol : repo.single_policies) {
            state_for_a_policy(pol);
        }

        std::uint32_t range_policies_count = static_cast<std::uint32_t>(repo.policies_range.size());
        seri.absorb(range_policies_count);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            repo.policies_range.resize(range_policies_count);
        }

        for (auto &pol : repo.policies_range) {
            state_for_a_policy(pol);
        }

        epoc::absorb_des(&repo.default_policy.read_access, seri);
        epoc::absorb_des(&repo.default_policy.write_access, seri);

        if (repo.ver >= 2) {
            seri.absorb(repo.default_policy.high_key);
            seri.absorb(repo.default_policy.key_mask);
        }

        seri.absorb(repo.default_meta);

        auto do_state_for_a_metadata_entry = [&](central_repo_default_meta &meta) {
            seri.absorb(meta.low_key);
            seri.absorb(meta.high_key);
            seri.absorb(meta.key_mask);
            seri.absorb(meta.default_meta_data);
        };

        std::uint32_t default_meta_range_count = static_cast<std::uint32_t>(repo.meta_range.size());
        seri.absorb(default_meta_range_count);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            repo.meta_range.resize(default_meta_range_count);
        }

        for (auto &meta : repo.meta_range) {
            do_state_for_a_metadata_entry(meta);
        }

        seri.absorb(repo.time_stamp);

        // Start to read entries
        std::uint32_t num_entries = static_cast<std::uint32_t>(repo.entries.size());
        seri.absorb(num_entries);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            repo.entries.resize(num_entries);
        }

        for (auto &entry : repo.entries) {
            seri.absorb(entry.key);
            seri.absorb(entry.metadata_val);

            std::uint8_t entry_type = 0;

            if (seri.get_seri_mode() != common::SERI_MODE_READ) {
                switch (entry.data.etype) {
                case central_repo_entry_type::integer: {
                    entry_type = 0;
                    break;
                }

                case central_repo_entry_type::real: {
                    entry_type = 1;
                    break;
                }

                case central_repo_entry_type::string: {
                    entry_type = 2;
                    break;
                }

                default:
                    break;
                }
            }

            seri.absorb(entry_type);

            switch (entry_type) {
            case 0: {
                entry.data.etype = central_repo_entry_type::integer;
                break;
            }

            case 1: {
                entry.data.etype = central_repo_entry_type::real;
                break;
            }

            case 2:
            case 3: {
                entry.data.etype = central_repo_entry_type::string;
                break;
            }

            default: {
                entry.data.etype = central_repo_entry_type::none;
                break;
            }
            }

            switch (entry.data.etype) {
            case central_repo_entry_type::integer: {
                std::uint32_t data = static_cast<std::uint32_t>(entry.data.intd);
                seri.absorb(data);

                entry.data.intd = data;

                break;
            }

            case central_repo_entry_type::real: {
                float dat = static_cast<float>(entry.data.reald);
                seri.absorb_impl(reinterpret_cast<std::uint8_t *>(&dat), sizeof(float));

                entry.data.reald = dat;

                break;
            }

            case central_repo_entry_type::string: {
                epoc::absorb_des_string(entry.data.strd, seri);
                break;
            }

            default:
                break;
            }
        }

        if (repo.ver >= 1) {
            std::uint32_t deleted_settings_count = static_cast<std::uint32_t>(repo.deleted_settings.size());
            seri.absorb(deleted_settings_count);

            if (seri.get_seri_mode() == common::SERI_MODE_READ) {
                repo.deleted_settings.resize(deleted_settings_count);
            }

            for (auto &deleted_setting : repo.deleted_settings) {
                seri.absorb(deleted_setting);
            }
        }

        // TODO: Supply policy for entry

        return 0;
    }

    void central_repo::write_changes(eka2l1::io_system *io, manager::device_manager *mngr) {
        std::vector<std::uint8_t> bufs;

        {
            common::chunkyseri seri(nullptr, 0, common::SERI_MODE_MEASURE);
            do_state_for_cre(seri, *this);

            bufs.resize(seri.size());
        }

        common::chunkyseri seri(&bufs[0], bufs.size(), common::SERI_MODE_WRITE);
        do_state_for_cre(seri, *this);

        std::u16string p{ drive_to_char16(reside_place) };
        std::u16string firm_code = common::utf8_to_ucs2(common::lowercase_string(mngr->get_current()->firmware_code));

        p += u":\\Private\\10202BE9\\persists\\" + firm_code + u"\\" + common::utf8_to_ucs2(common::to_string(uid, std::hex)) + u".cre";

        symfile f = io->open_file(p, WRITE_MODE | BIN_MODE);

        if (!f) {
            LOG_ERROR("Can't write CRE changes, opening {} failed", common::ucs2_to_utf8(p));
            return;
        }

        f->write_file(&bufs[0], 1, static_cast<std::uint32_t>(bufs.size()));
        f->close();
    }

    void central_repo_client_subsession::write_changes(eka2l1::io_system *io, manager::device_manager *mngr) {
        attach_repo->write_changes(io, mngr);
    }
}
