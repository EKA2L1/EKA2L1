/*
 * Copyright (c) 2020 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project.
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

#include <services/ui/cap/coestorage.h>
#include <services/ui/cap/consts.h>

#include <services/centralrepo/centralrepo.h>

#include <common/cvt.h>
#include <common/log.h>

namespace eka2l1::epoc {
    coe_data_storage::coe_data_storage(eka2l1::central_repo_server *serv, io_system *io, device_manager *mngr)
        : serv_(serv)
        , fep_repo_(nullptr)
        , dmngr_(mngr)
        , io_(io) {
    }

    eka2l1::central_repo *coe_data_storage::fep_repo() {
        if (!fep_repo_) {
            fep_repo_ = serv_->load_repo_with_lookup(io_, dmngr_, FEP_FRAMEWORK_REPO_UID);
        }

        return fep_repo_;
    }

    static eka2l1::central_repo_entry *get_ccr_entry(eka2l1::central_repo *rep, const std::uint32_t key,
        const eka2l1::central_repo_entry_type target) {
        if (!rep) {
            return nullptr;
        }

        eka2l1::central_repo_entry *ent = rep->find_entry(key);
        if (!ent || (ent->data.etype != target)) {
            return nullptr;
        }

        return ent;
    }

    std::optional<std::u16string> coe_data_storage::default_fep() {
        eka2l1::central_repo *rep = fep_repo();
        eka2l1::central_repo_entry *ccre = get_ccr_entry(rep, fep_framework_repo_key_default_fepid,
            eka2l1::central_repo_entry_type::string16);
        
        if (!ccre) {
            return std::nullopt;
        }

        return ccre->data.str16d;
    }

    void coe_data_storage::default_fep(const std::u16string &the_fep) {
        eka2l1::central_repo *rep = fep_repo();
        eka2l1::central_repo_entry *ccre = get_ccr_entry(rep, fep_framework_repo_key_default_fepid,
            eka2l1::central_repo_entry_type::string16);

        if (!ccre) {
            eka2l1::central_repo_entry_variant var;
            var.etype = central_repo_entry_type::string16;
            var.str16d = the_fep;

            if (!rep->add_new_entry(fep_framework_repo_key_default_fepid, var)) {
                LOG_WARN(SERVICE_UI, "Unable to add default fepid entry to cenrep");
            }
        } else {
            ccre->data.str16d = the_fep;
        }
    }

    void coe_data_storage::serialize() {
        eka2l1::central_repo *rep = fep_repo();
        
        if (rep) {
            rep->write_changes(io_, dmngr_);
        }
    }
}