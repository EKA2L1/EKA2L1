/*
 * Copyright (c) 2018 EKA2L1 Team
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

#pragma once

#include <cstdint>

namespace eka2l1 {
    enum applist_servs_range {
        applist_range_unrestricted = 0,
        applist_range_write_device_data_cap = 100,
    };

    enum applist_request_oldarch {
        applist_request_oldarch_first = applist_range_unrestricted, // = 0
        applist_request_oldarch_init_list,
        applist_request_oldarch_get_next_app,
        applist_request_oldarch_embed_count,
        applist_request_oldarch_app_count,
        applist_request_oldarch_app_info,
        applist_request_oldarch_get_app_capability,
        applist_request_oldarch_start_app,
        applist_request_oldarch_recognize_data,
        applist_request_oldarch_recognize_specific_data,
        applist_request_oldarch_app_for_data_type,      // = 10
        applist_request_oldarch_start_document,
        applist_request_oldarch_start_document_by_data_type,
        applist_request_oldarch_start_document_by_uid,
        applist_request_oldarch_create_document_by_uid,
        applist_request_oldarch_app_icon_by_uid,
        applist_request_oldarch_app_for_document,
        applist_request_oldarch_is_program,
        applist_request_oldarch_get_confidence,
        applist_request_oldarch_set_accepted_confidence,
        applist_request_oldarch_get_buf_size,   // = 20
        applist_request_oldarch_set_buf_size,
        applist_request_oldarch_get_data_types_phase1,
        applist_request_oldarch_get_data_types_phase2,
        applist_request_oldarch_set_notify,
        applist_request_oldarch_cancel_notify,
        applist_request_oldarch_close_server,
        applist_request_oldarch_app_icon_by_uid_and_size,
        applist_request_oldarch_get_app_icon_sizes,
        applist_request_oldarch_get_app_views,
        applist_request_oldarch_view_icon_by_uid_and_size,  // = 30
        applist_request_oldarch_get_file_ownership_infos
    };

    enum applist_request_newarch {
        applist_request_first = applist_range_unrestricted, // = 0
        applist_request_init_full_list,
        applist_request_init_embed_list,
        applist_request_get_next_app,
        applist_request_embed_count,
        applist_request_app_count,
        applist_request_app_info,
        applist_request_app_capability,
        applist_request_start_app_without_returning_thread_id, // deprecated
        applist_request_start_app_returning_thread_id, // deprecated
        applist_request_recognize_data, // = 10
        applist_request_recognize_data_passed_by_file_handle,
        applist_request_recognize_specific_data,
        applist_request_recognize_specific_data_passed_by_file_handle,
        applist_request_app_for_data_type,
        applist_request_start_document, // deprecated
        applist_request_start_document_by_data_type, // deprecated
        applist_request_start_document_by_uid, // deprecated
        applist_request_create_document_by_uid, // deprecated
        applist_request_get_executable_name_given_document,
        applist_request_get_executable_name_given_document_passed_by_file_handle, // = 20
        applist_request_get_executable_name_given_data_type,
        applist_request_get_executable_name_given_app_uid,
        applist_request_get_opaque_data,
        applist_request_get_executable_name_if_non_native,
        applist_request_app_icon_by_uid,
        applist_request_app_for_document,
        applist_request_app_for_document_passed_by_file_handle,
        applist_request_app_get_confidence,
        applist_request_get_buf_size,
        applist_request_set_buf_size, // = 30
        applist_request_get_data_types_phase1,
        applist_request_get_data_types_phase2,
        applist_request_set_notify,
        applist_request_cancel_notify,
        applist_request_dclose_server,
        applist_request_app_icon_by_uid_and_size,
        applist_request_get_app_icon_sizes,
        applist_request_get_app_views,
        applist_request_view_icon_by_uid_and_size,
        applist_request_get_ownership_infos, // = 40
        applist_request_number_of_own_defined_icons,
        applist_request_init_embed_filtered_list,
        applsit_request_init_attr_filtered_list,
        applist_request_app_icon_filename,
        applist_request_view_icon_filename,
        applist_request_init_server_applist,
        applist_request_get_app_services,
        applist_request_get_service_implementations,
        applist_request_get_service_implementations_data_types,
        applist_request_get_app_services_uid, // = 50
        applist_request_get_app_service_opaque_type,
        applist_request_app_for_data_type_and_service,
        applist_request_app_for_document_and_service,
        applist_request_app_for_document_and_service_passed_by_file_handle,
        applist_request_app_language,
        applist_request_register_list_population_complete_observer,
        applist_request_cancel_list_population_complete_observer,
        applist_request_preferred_buf_size,
        applist_request_recognize_files,
        applist_request_recognize_files_as_async, // = 60
        applist_request_cancel_recognise_files,
        applist_request_transfer_recognistion_result,
        applist_request_get_app_by_data_type,
        applist_request_get_default_screen_number,
        applist_request_rule_based_launching,
        applist_request_register_non_native_app_type,
        applist_request_deregister_non_native_app_type,
        applist_request_prepare_non_native_app_updates,
        applist_request_register_non_native_app,
        applist_request_deregister_non_native_app, // = 70
        applist_request_commit_non_native_application,
        applist_request_rollback_non_native_application,
        applist_request_get_app_type,
        applist_request_notify_on_data_mapping_change,
        applist_request_cancel_request_notify_on_data_mapping_change,
        applist_request_matches_security_policy,
        applist_request_set_app_short_caption,
        applist_request_force_registeration,
        applist_request_debug_mark_heap,
        applist_request_debug_mark_heap_end, // = 80
        applist_request_debug_heap_fail_next,
        applist_request_debug_clear_app_info_array,
        applist_request_debug_flush_recognition_cache,
        applist_request_debug_set_load_recognizers_on_demand,
        applist_request_debug_perform_outstanding_recognizer_unloading,
        applist_request_app_icon_file_handle,
        applist_request_debug_add_failing_non_native_apps_update,
        applist_request_add_panicing_failing_non_native_apps_update,
        applist_request_rollback_panicing_failing_non_native_apps_update, // = 89
        applist_request_update_applist,
        applist_request_update_app_infos,
        applist_request_app_info_provide_by_reg_file = 99, // = 99
        //WriteDeviceData Capability requirement
        // ER5
        applist_request_set_confidence = applist_range_write_device_data_cap, // = 100
        // 8.1
        applist_request_insert_data_mapping,
        applist_request_insert_data_mapping_if_higher,
        applist_request_delete_data_mapping,
    };
}