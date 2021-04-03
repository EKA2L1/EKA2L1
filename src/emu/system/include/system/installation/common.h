#pragma once

namespace eka2l1 {
    enum device_installation_error {
        device_installation_none = 0,
        device_installation_not_exist,
        device_installation_insufficent,
        device_installation_rpkg_corrupt,
        device_installation_determine_product_failure,
        device_installation_already_exist,
        device_installation_no_languages_present,
        device_installation_general_failure,
        device_installation_raw_dump_fail_to_copy,
        device_installation_rom_fail_to_copy
    };
}