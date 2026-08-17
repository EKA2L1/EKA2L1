#pragma once

namespace eka2l1 {
    enum device_installation_error {
        device_installation_none = 0,
        device_installation_not_exist,
        device_installation_insufficent,
        device_installation_rpkg_corrupt,
        device_installation_determine_product_failure,
        device_installation_already_exist,
        device_installation_general_failure,
        device_installation_rom_fail_to_copy,
        device_installation_vpl_file_invalid,
        device_installation_rofs_corrupt,
        device_installation_rom_file_corrupt,
        device_installation_fpsx_corrupt,
        // The ROM needs an RPKG to populate drive Z, but none was supplied.
        device_installation_rpkg_missing,
        // An archive of device files could not be opened or decompressed.
        device_installation_archive_corrupt,
        // The archive opened fine but holds nothing that looks like a device dump.
        device_installation_archive_no_device
    };
}