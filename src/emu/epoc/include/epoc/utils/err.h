#pragma once

#include <cstdint>

namespace eka2l1::epoc {
    enum error : std::int32_t {
        error_none = 0, ///< No error.
        error_not_found = -1, ///< Item not found.
        error_general = -2, ///< Error with no specific description.
        error_cancel = -3, ///< An operation has been canceled.
        error_no_memory = -4, ///< Not enough memory for an operation.
        error_not_supported = -5, ///< A feature is not supported.
        error_argument = -6, ///< An argument is fake.
        error_total_lost_precision = -7, ///< A calculation has lost precision.
        error_bad_handle = -8, ///< An invalid handle is passed.
        error_overflow = -9, ///< Overflow in some operations.
        error_underflow = -10, ///< Underflow in some operations.
        error_already_exists = -11, ///< An object already exists
        error_path_not_found = -12, ///< A path was not found.
        error_died = -13, ///< Literally it.
        error_in_use = -14, ///< A resource is in use.
        error_server_terminated = -15, ///< Server side has been shutted down.
        error_server_busy = -16, ///< Server is busy
        error_completion = -17, ///< An operation is completed.
        error_not_ready = -18, ///< A unit is not ready for operations.
        error_unknown = -19, ///< Error of unknown type.
        error_corrupt = -20, ///< Indicates corruption.
        error_access_denied = -21, ///< Access to something is not allowed.
        error_locked = -22, ///< Access to something is locked.
        error_write = -23, ///< During file operation, not all data could be written.
        error_dismounted = -24, ///< A volume has been dismounted.
        error_eof = -25, ///< Indicates end of file.
        error_disk_full = -26, ///< The disk is full.
        error_bad_driver = -27, ///< A driver DLL is of the wrong type.
        error_bad_name = -28, ///< A name is not valid (to belong category).
        error_comms_line_fail = -29, ///< Communication line failed.
        error_comms_frame = -30, ///< Frame error happens in communication operation.
        error_comms_overrun = -31, ///< An overrun detected by communication driver.
        error_comms_parity = -32, ///< A parity error happens in communication operation.
        error_timed_out = -33, ///< An operations has timed out.
        error_could_not_connect = -34, ///< A session could not connect.
        error_could_not_disconnect = -35, ///< A session could not disconnect.
        error_disconnected = -36, ///< An operation can't be done due to session being disconnected.
        error_bad_lib_entry_point = -37, ///< A library entry point was not the required type.
        error_bad_descriptor = -38, ///< An descriptor is corrupted.
        error_abort = -39, ///< An operation has been aborted.
        error_too_big = -40, ///< Something is too big.
        error_divide_by_zero = -41, ///< A divide-by-zero operation has been attempted.
        error_bad_power = -42, ///< Insufficent power to finish an operation.
        error_dir_full = -43, ///< An operation with directory has failed.
        error_hardware_not_avail = -44, ///< Neccessary hardware is not available for an operation.
        error_session_closed = -45, ///< Shared session has been closed.
        error_permission_denied = -46, ///< Not enough permission to perform an operation.
        error_ext_not_supported = -47, ///< An extension is not supported by an operation.
        error_comms_break = -48, ///< A break happens in a comms operation.
        error_no_secure_time = -49, ///< Trusted time source could not be found.
        error_surrogate_found = -50, ///< Corrupt surrogate is found when processing a text/descriptor buffer.
    };
}