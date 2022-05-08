// Copyright (c) 1999-2010 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of "Eclipse Public License v1.0"
// which accompanies this distribution, and is available
// at the URL "http://www.eclipse.org/legal/epl-v10.html".
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
//
// Contributors:
//
// Description:
// Command numbers and structures for client to window server communications.
//
//

#pragma once

namespace eka2l1 {
    enum ws_messages {
        ws_mess_command_buffer = 0,
        ws_mess_shutdown = 1,
        ws_mess_init = 2,
        ws_mess_finish = 3,
        ws_mess_sync_msg_buf = 4,
        ws_mess_async_service = 0x010000,
        ws_mess_anim_dll_async_cmd = 0x100000
    };

    enum ws_client_opcodes {
        ws_cl_op_disconnect = 0,
        ws_cl_op_clear_hot_keys = 1,
        ws_cl_op_set_hot_key = 2,
        ws_cl_op_restore_default_hotkey = 3,
        ws_cl_op_compute_mode = 4,
        ws_cl_op_event_ready = 5,
        ws_cl_op_event_ready_cancel = 6,
        ws_cl_op_get_event = 7,
        ws_cl_op_purge_pointer_events = 8,
        ws_cl_op_redraw_ready = 9,
        ws_cl_op_redraw_ready_cancel = 10,
        ws_cl_op_get_redraw = 11,
        ws_cl_op_priority_key_ready = 12,
        ws_cl_op_priority_key_ready_cancel = 13,
        ws_cl_op_get_priority_key = 14,
        ws_cl_op_set_shadow_vector = 15,
        ws_cl_op_shadow_vector = 16,
        ws_cl_op_set_keyboard_repeat_rate = 17,
        ws_cl_op_get_keyboard_repeat_rate = 18,
        ws_cl_op_set_double_click = 19,
        ws_cl_op_get_double_click_settings = 20,
        ws_cl_op_create_window_group = 21,
        ws_cl_op_create_window = 22,
        ws_cl_op_create_gc = 23,
        ws_cl_op_create_anim_dll = 24,
        ws_cl_op_create_screen_device = 25,
        ws_cl_op_create_sprite = 26,
        ws_cl_op_create_pointer_cursor = 27,
        ws_cl_op_start_custom_text_cursor = 28,
        ws_cl_op_complete_custom_text_cursor = 29,
        ws_cl_op_create_bitmap = 30,
        ws_cl_op_num_window_groups = 31,
        ws_cl_op_num_groups_all_priorities = 32,
        ws_cl_op_window_group_list = 33,
        ws_cl_op_window_group_list_all_priorities = 34,
        ws_cl_op_set_window_group_ordinal_position = 35,
        ws_cl_op_get_window_group_handle = 36,
        ws_cl_op_get_window_group_ordinal_priority = 37,
        ws_cl_op_get_window_group_client_thread_id = 38,
        ws_cl_op_get_window_group_name_from_identifier = 39,
        ws_cl_op_get_focus_window_group = 40,
        ws_cl_op_get_default_owning_window = 41,
        ws_cl_op_send_event_to_window_group = 42,
        ws_cl_op_find_window_group_identifier = 43,
        ws_cl_op_find_window_group_identifier_thread = 44,
        ws_cl_op_set_background_color = 45,
        ws_cl_op_get_background_color = 46,
        ws_cl_op_set_system_pointer_cursor = 47,
        ws_cl_op_clear_system_pointer_cursor = 48,
        ws_cl_op_claim_system_pointer_cursor_list = 49,
        ws_cl_op_free_system_pointer_cursor_list = 50,
        ws_cl_op_get_modifier_state = 51,
        ws_cl_op_set_modifier_state = 52,
        ws_cl_op_heap_count = 53,
        ws_cl_op_restore_count = 54,
        ws_cl_op_test_variant = 55,
        ws_cl_op_heap_set_fail = 56,
        ws_cl_op_raw_event = 57,
        ws_cl_op_system_info = 58,
        ws_cl_op_get_message = 59,
        ws_cl_op_password_entered = 60,
        ws_cl_op_send_message_to_window_group = 61,
        ws_cl_op_send_off_events_to_shell = 62,
        ws_cl_op_get_def_mode_max_num_colors = 63,
        ws_cl_op_get_color_mode_list = 64,
        ws_cl_op_key_event = 65,
        ws_cl_op_send_event_to_all_window_groups = 66,
        ws_cl_op_send_event_to_all_window_group_priority = 67,
        ws_cl_op_set_pointer_cursor_area = 68,
        ws_cl_op_pointer_cursor_area = 69,
        ws_cl_op_set_pointer_cursor_mode = 70,
        ws_cl_op_pointer_cursor_mode = 71,
        ws_cl_op_set_default_system_pointer_cursor = 72,
        ws_cl_op_clear_default_system_pointer_cursor = 73,
        ws_cl_op_set_pointer_cursor_position = 74,
        ws_cl_op_pointer_cursor_position = 75,
        ws_cl_op_set_default_fading_params = 76,
        ws_cl_op_send_message_to_all_window_groups = 77,
        ws_cl_op_send_message_to_all_window_groups_priority = 78,
        ws_cl_op_fetch_message = 79,
        ws_cl_op_prepare_for_switch_off = 80,
        ws_cl_op_create_direct_screen_access = 81,
        ws_cl_op_set_faded = 82,
        ws_cl_op_log_command = 83,
        ws_cl_op_create_click = 84,
        ws_cl_op_send_event_to_one_window_group_per_cli = 85,
        ws_cl_op_no_flicker_free = 86,
        ws_cl_op_set_focus_screen = 87,
        ws_cl_op_get_focus_screen = 88,
        ws_cl_op_window_group_list_and_chain = 89,
        ws_cl_op_window_group_list_and_chain_all_priorities = 90,
        ws_cl_op_set_client_cursor_mode = 91,
        ws_cl_op_clear_all_redraw_stores = 92,
        ws_cl_op_create_graphics = 93,
        ws_cl_op_graphics_message_ready = 94,
        ws_cl_op_get_graphic_message = 95,
        ws_cl_op_graphic_message_cancel = 96,
        ws_cl_op_num_window_groups_on_screen = 97,
        ws_cl_op_get_number_screen = 98,
        ws_cl_op_graphic_abort_message = 99,
        ws_cl_op_graphic_fetch_header_message = 100,
        ws_cl_op_debug_info = 101,
        ws_cl_op_debug_info_reply_buf = 102,
        ws_cl_op_register_surface = 103,
        ws_cl_op_unregister_surface = 104,
        ws_cl_op_set_close_proximity_thresholds = 105,
        ws_cl_op_set_high_pressure_thresholds = 106,
        ws_cl_op_get_enter_close_proximity_threshold = 107,
        ws_cl_op_get_exit_close_proximity_threshold = 108,
        ws_cl_op_get_enter_high_pressure_thresholds = 109,
        ws_cl_op_get_exit_high_pressure_thresholds = 110,
        ws_cl_op_create_drawable_source = 111,
        ws_cl_op_create_dsa_for_region_trackin_only = 112,
        ws_cl_op_send_effect_command = 113,
        ws_cl_op_register_tfx_effect_buf = 114,
        ws_cl_op_register_tfx_effect_ipc = 115,
        ws_cl_op_unregister_tfx_effect = 116,
        ws_cl_op_unregister_all_tfx_effect = 117,
        ws_cl_op_override_effect_buf = 118,
        ws_cl_op_override_effect_ipc = 119,
        ws_cl_op_indicate_app_orientation = 120
    };

    enum ws_screen_device_opcode {
        ws_sd_op_setable_bit_flag = 0x80000000,
        ws_sd_op_get_scan_line = 0x0000,
        ws_sd_op_pixel = 1,
        ws_sd_op_twips_size = 2,
        ws_sd_op_pixel_size = 3,
        ws_sd_op_free = 4,
        ws_sd_op_horizontal_twips_to_pixels = 5,
        ws_sd_op_vertical_twips_to_pixels = 6,
        ws_sd_op_horizontal_pixels_to_twips = 7,
        ws_sd_op_vertical_pixels_to_twips = 8,
        ws_sd_op_display_mode = 9,
        ws_sd_op_rect_compare = 10,
        ws_sd_op_pointer_rect = 11,
        ws_sd_op_copy_screen_to_bitmap = 12,
        ws_sd_op_copy_screen_to_bitmap2 = 13,
        ws_sd_op_set_screen_size_and_rotation = 14,
        ws_sd_op_set_screen_size_and_rotation2 = 15,
        ws_sd_op_get_default_screen_size_and_rotation = 16,
        ws_sd_op_get_default_screen_size_and_rotation2 = 17,
        ws_sd_op_get_num_screen_modes = 18,
        ws_sd_op_set_screen_mode = 19,
        ws_sd_op_get_screen_mode_size_and_rotation = 20,
        ws_sd_op_get_screen_mode_size_and_rotation2 = 21,
        ws_sd_op_set_screen_mode_enforcement = 22,
        ws_sd_op_screen_mode_enforcement = 23,
        ws_sd_op_set_mode_rotation = 24,
        ws_sd_op_get_rotation_list = 25,
        ws_sd_op_palette_attributes = 26,
        ws_sd_op_set_palette = 27,
        ws_sd_op_get_palette = 28,
        ws_sd_op_get_screen_mode = 29,
        ws_sd_op_get_default_screen_mode_origin = 30,
        ws_sd_op_get_screen_mode_origin = 31,
        ws_sd_op_get_screen_mode_scale = 32,
        ws_sd_op_get_current_screen_mode_scale = 33,
        ws_sd_op_set_app_screen_mode = 34,
        ws_sd_op_get_screen_mode_scaled_origin = 35,
        ws_sd_op_get_current_screen_mode_scaled_origin = 36,
        ws_sd_op_set_current_screen_mode_attributes = 37,
        ws_sd_op_get_current_screen_mode_attributes = 38,
        ws_sd_op_get_screen_number = 39,
        ws_sd_op_get_screen_size_mode_list = 40,
        ws_sd_op_get_screen_mode_display_mode = 41,
        ws_cl_op_set_backlight = 42, // Duh what
        ws_sd_op_extension_supported = 43,
        ws_sd_op_XDc_get_number_resolutions = 44,
        ws_sd_op_XDc_get_resolution_list = 45,
        ws_sd_op_XDc_get_config = 46,
        ws_sd_op_XDc_set_config = 47,
        ws_sd_op_XDc_get_preferred_display_version = 48,
        ws_sd_op_XDc_notify_on_display_change = 49,
        ws_sd_op_XDc_notify_on_display_change_cancel = 50,
        ws_sd_op_XDc_display_change_event_enable = 51,
        ws_sd_op_XDm_map_extent = 52,
        ws_sd_op_XTest_screen_capture = 53,
        ws_sd_op_is_screen_mode_dynamic = 54,
        ws_sd_op_test_screen_capture_size = 55,
    };

    enum ws_dsa_op {
        ws_dsa_free = 0,
        ws_dsa_request = 1,
        ws_dsa_init_failed = 2,
        ws_dsa_get_region = 3,
        ws_dsa_cancel = 4,
        ws_dsa_get_send_queue = 5,
        ws_dsa_get_rec_queue = 6,
        ws_dsa_get_sync_thread = 7
    };

    enum ws_dsa_op_old {
        ws_dsa_old_free = 0,
        ws_dsa_old_get_sync_thread = 1,
        ws_dsa_old_request = 2,
        ws_dsa_old_init_failed = 3,
        ws_dsa_old_get_region = 4,
        ws_dsa_old_cancel = 5
    };

    enum ws_sprite_op {
        ws_sprite_free = 0,
        ws_sprite_set_position = 1,
        ws_sprite_update_member = 2,
        ws_sprite_update_member2 = 3,
        ws_sprite_activate = 4,
        ws_sprite_append_member = 5
    };

    enum ws_click_op {
        ws_click_free = 0,
        ws_click_is_loaded = 1,
        ws_click_unload = 2,
        ws_click_load = 3,
        ws_click_command_reply = 4,
        ws_click_set_key_click = 5,
        ws_click_set_pen_click = 6,
        ws_click_key_click_enabled = 7,
        ws_click_pen_click_enabled = 8
    };

    enum ws_graphic_drawer_opcode {
        ws_graphic_drawer_free = 0,
        ws_graphic_drawer_send_msg = 1,
        ws_graphic_drawer_get_graphic_id = 2,
        ws_graphic_drawer_share_globally = 3,
        ws_graphic_drawer_unshare_globally = 4,
        ws_graphic_drawer_share = 5,
        ws_graphic_drawer_unshare = 6,
        ws_graphic_drawer_send_sync = 7
    };

#define OPCODE_NAME(aaaprefix, aname, aversion) aaaprefix##_##aversion##_##aname
#define OPCODE(aprefix, aname, aversion, val) \
    OPCODE_NAME(aprefix, aname, aversion) = val,

#define OPCODE2(aprefix, aname, version1, val1, version2, val2) \
    OPCODE(aprefix, aname, version1, val1)                      \
    OPCODE(aprefix, aname, version2, val2)

#define OPCODE3(aprefix, aname, version1, val1, version2, val2, version3, val3) \
    OPCODE(aprefix, aname, version1, val1)                                      \
    OPCODE(aprefix, aname, version2, val2)                                      \
    OPCODE(aprefix, aname, version3, val3)

#define OPCODE4(aprefix, aname, version1, val1, version2, val2, version3, val3, version4, val4) \
    OPCODE(aprefix, aname, version1, val1)                                                      \
    OPCODE(aprefix, aname, version2, val2)                                                      \
    OPCODE(aprefix, aname, version3, val3)                                                      \
    OPCODE(aprefix, aname, version4, val4)

    enum ws_graphics_context_opcode {
#include <services/window/gcop.def>
    };

#undef OPCODE_NAME
#undef OPCODE
#undef OPCODE2
#undef OPCODE3
#undef OPCODE4

    enum ws_anim_dll_opcode {
        ws_anim_dll_op_create_instance = 0,
        ws_anim_dll_op_command = 1,
        ws_anim_dll_op_command_reply = 2,
        ws_anim_dll_op_destroy_instance = 3,
        ws_anim_dll_op_free = 4,
        ws_anim_dll_op_create_instance_sprite = 5
    };
}

enum TWsWindowOpcodes {
    EWsWinOpFree = 0x0000,
    EWsWinOpSetExtent,
    EWsWinOpSetExtentErr,
    EWsWinOpOrdinalPosition,
    EWsWinOpFullOrdinalPosition,
    EWsWinOpSetOrdinalPosition,
    EWsWinOpSetOrdinalPositionPri,
    EWsWinOpSetOrdinalPriorityAdjust,
    EWsWinOpSetPos,
    EWsWinOpSetSize,
    EWsWinOpSetSizeErr,
    EWsWinOpPosition,
    EWsWinOpAbsPosition,
    EWsWinOpSize,
    EWsWinOpActivate,
    EWsWinOpInvalidate,
    EWsWinOpInvalidateFull,
    EWsWinOpBeginRedraw,
    EWsWinOpBeginRedrawFull,
    EWsWinOpEndRedraw,
    EWsWinOpTestInvariant,
    EWsWinOpParent,
    EWsWinOpPrevSibling,
    EWsWinOpNextSibling,
    EWsWinOpChild,
    EWsWinOpInquireOffset,
    EWsWinOpPointerFilter,
    EWsWinOpSetPointerCapture,
    EWsWinOpSetPointerGrab,
    EWsWinOpClaimPointerGrab,
    EWsWinOpSetBackgroundColor,
    EWsWinOpSetOrdinalPositionErr,
    EWsWinOpSetVisible,
    EWsWinOpScroll,
    EWsWinOpScrollClip,
    EWsWinOpScrollRect,
    EWsWinOpScrollClipRect,
    EWsWinOpReceiveFocus,
    EWsWinOpAutoForeground,
    EWsWinOpSetNoBackgroundColor,
    EWsWinOpCaptureKey,
    EWsWinOpCancelCaptureKey,
    EWsWinOpCaptureKeyUpsAndDowns,
    EWsWinOpCancelCaptureKeyUpsAndDowns,
    EWsWinOpAddPriorityKey,
    EWsWinOpRemovePriorityKey,
    EWsWinOpSetTextCursor,
    EWsWinOpSetTextCursorClipped,
    EWsWinOpCancelTextCursor,
    EWsWinOpSetShadowHeight,
    EWsWinOpShadowDisabled,
    EWsWinOpGetInvalidRegion,
    EWsWinOpGetInvalidRegionCount,
    EWsWinOpSetColor,
    EWsWinOpSetCornerType,
    EWsWinOpSetShape,
    EWsWinOpBitmapHandle,
    EWsWinOpUpdateScreen,
    EWsWinOpUpdateScreenRegion,
    EWsWinOpUpdateBackupBitmap,
    EWsWinOpMaintainBackup,
    EWsWinOpName,
    EWsWinOpSetName,
    EWsWinOpSetOwningWindowGroup,
    EWsWinOpDefaultOwningWindow,
    EWsWinOpRequiredDisplayMode,
    EWsWinOpEnableOnEvents,
    EWsWinOpDisableOnEvents,
    EWsWinOpEnableGroupChangeEvents,
    EWsWinOpDisableGroupChangeEvents,
    EWsWinOpSetPointerCursor,
    EWsWinOpSetCustomPointerCursor,
    EWsWinOpRequestPointerRepeatEvent,
    EWsWinOpCancelPointerRepeatEventRequest,
    EWsWinOpAllocPointerMoveBuffer,
    EWsWinOpFreePointerMoveBuffer,
    EWsWinOpEnablePointerMoveBuffer,
    EWsWinOpDisablePointerMoveBuffer,
    EWsWinOpRetrievePointerMoveBuffer,
    EWsWinOpDiscardPointerMoveBuffer, //Tested to here %%%
    EWsWinOpEnableModifierChangedEvents,
    EWsWinOpDisableModifierChangedEvents,
    EWsWinOpEnableErrorMessages,
    EWsWinOpDisableErrorMessages,
    EWsWinOpAddKeyRect,
    EWsWinOpRemoveAllKeyRects,
    EWsWinOpPasswordWindow,
    EWsWinOpEnableBackup,
    EWsWinOpIdentifier,
    EWsWinOpDisableKeyClick,
    EWsWinOpSetFade = EWsWinOpDisableKeyClick + 3, //Two messages removed
    EWsWinOpSetNonFading,
    EWsWinOpFadeBehind,
    EWsWinOpEnableScreenChangeEvents,
    EWsWinOpDisableScreenChangeEvents,
    EWsWinOpSendPointerEvent,
    EWsWinOpSendAdvancedPointerEvent,
    EWsWinOpGetDisplayMode,
    EWsWinOpGetIsFaded,
    EWsWinOpGetIsNonFading,
    EWsWinOpOrdinalPriority,
    EWsWinOpClearPointerCursor,
    EWsWinOpMoveToGroup,
    EWsWinOpEnableFocusChangeEvents,
    EWsWinOpDisableFocusChangeEvents,
    EWsWinOpEnableGroupListChangeEvents,
    EWsWinOpDisableGroupListChangeEvents,
    EWsWinOpCaptureLongKey,
    EWsWinOpCancelCaptureLongKey,
    EWsWinOpStoreDrawCommands,
    EWsWinOpHandleTransparencyUpdate,
    EWsWinOpSetTransparencyFactor,
    EWsWinOpSetTransparencyBitmap,
    EWsWinOpAllowChildWindowGroup,
    EWsWinOpSetTransparencyBitmapCWs,
    EWsWinOpEnableVisibilityChangeEvents, // Is 0x74 on S60^5
    EWsWinOpDisableVisibilityChangeEvents,
    EWsWinOpSetTransparencyAlphaChannel,
    EWsWinOpWindowGroupId,
    EWsWinOpSetPointerCapturePriority,
    EWsWinOpGetPointerCapturePriority,
    EWsWinOpSetTransparentRegion,
    EWsWinOpSetTransparencyPolicy,
    EWsWinOpIsRedrawStoreEnabled,
    EWsWinOpEnableOSB,
    EWsWinOpDisableOSB,
    EWsWinOpClearChildGroup,
    EWsWinOpSetChildGroup,
    EWsWinOpClientHandle,
    EWsWinOpSetBackgroundSurface,
    EWsWinOpKeyColor = EWsWinOpSetBackgroundSurface + 2, //One message removed
    EWsWinOpSetBackgroundSurfaceConfig = EWsWinOpKeyColor + 5, //Four messages removed
    EWsWinOpRemoveBackgroundSurface = EWsWinOpSetBackgroundSurfaceConfig + 2, //One message removed
    EWsWinOpGetBackgroundSurfaceConfig = EWsWinOpRemoveBackgroundSurface + 2, //One message removed
    EWsWinOpClearRedrawStore = EWsWinOpGetBackgroundSurfaceConfig + 2, //One message removed
    EWsWinOpScreenNumber,
    EWsWinOpEnableAdvancedPointers,
#ifdef SYMBIAN_GRAPHICS_WSERV_QT_EFFECTS
    EWsWinOpSetSurfaceTransparency,
#endif
    EWsWinOpSetPurpose,
    EWsWinOpSendEffectCommand,
    EWsWinOpOverrideEffectBuf,
    EWsWinOpOverrideEffectIPC,
    EWsWinOpFixNativeOrientation = 0x99,
    EWsWinOpTestLowPriorityRedraw = 0x2000, //Specific opcode for testing redraw queue priorities
};

enum THotKey {
    /** Enables logging of all messages to and from the window server.
	Note that the required type of logging must have been specified in the wsini.ini
	file (using the LOG keyword), and the appropriate logging DLL must be available.
	Default key mapping: \<Ctrl\>\<Alt\>\<Shift\>E */
    EHotKeyEnableLogging,
    /** Always disables window server logging, if active. Does nothing otherwise.
	Default key mapping: \<Ctrl\>\<Alt\>\<Shift\>D */
    EHotKeyDisableLogging,
    /** Dumps a list of all windows to the log. (If logging is disabled, it is temporarily
	enabled in order to do this.)
	Default key mapping: \<Ctrl\>\<Alt\>\<Shift\>W */
    EHotKeyStateDump,
    /** Kills the foreground application.
	Default key mapping: \<Ctrl\>\<Alt\>\<Shift\>K */
    EHotKeyOfDeath,
    /** Shuts down the window server.
	Be cautious! This may mean resetting the machine to re-start the window server.
	Default key mapping: release (not available), debug (\<Ctrl\>\<Alt\>\<Shift\>X). */
    EHotKeyShutDown,
    /** Dumps a list of cells allocated on the window server's heap to the log. (If
	logging is disabled, it is temporarily enabled in order to do this.)
	Note that logging requires that the type of logging has been set up in the
	wsini.ini file, and that the appropriate logging DLL is available.
	Default key mapping: \<Ctrl\>\<Alt\>\<Shift\>H */
    EHotKeyHeapDump,
    /** Increases the LCD contrast.
	Default key mapping: EKeyIncContrast. Note that this value is from an enum
	in e32keys.h. The contrast wraps around when it reaches the maximum. */
    EHotKeyIncContrast,
    /** Decreases the LCD contrast.
	Default key mapping: EKeyDecContrast. Note that this value is from an enum
	in e32keys.h. The contrast wraps around when it reaches the minimum. */
    EHotKeyDecContrast,
    /** Switches the machine off.
	Default key mapping: EKeyOff. Note that this value is from an enum in e32keys.h. */
    EHotKeyOff,
    /** Switches the backlight on.
	Default key mapping: EKeyBacklightOn. Note that this value is from an enum
	in e32keys.h. */
    EHotKeyBacklightOn,
    /** Switches the backlight off.
	Default key mapping: EKeyBacklightOff. Note that this value is from an enum
	in e32keys.h. */
    EHotKeyBacklightOff,
    /** Toggles the backlight.
	Default key mapping: EKeyBacklightToggle. Note that this value is from an
	enum in e32keys.h. */
    EHotKeyBacklightToggle,
    /** Switches to screen size 0.
	This, and the following 3 keys are used to switch screen sizes on real hardware,
	for instance when the cover is closed on a phone that supports screen flipping. */
    EHotKeyScreenDimension0,
    /** Switches to screen size 1.
	This might be generated when the cover is opened on a phone that supports screen
	flipping. */
    EHotKeyScreenDimension1,
    /** Switches to screen size 2. */
    EHotKeyScreenDimension2,
    /** Switches to screen size 3. */
    EHotKeyScreenDimension3,
    /** Cycles the display though its possible sizes.
	This is used only for debugging.
	A device may have several screen sizes, each with a default orientation. For
	example a phone that supports screen flipping will have different display
	sizes when the cover is opened and closed.
	Default key mapping: debug : \<Ctrl\>\<Alt\>\<Shift\> U. Release : none. */
    EHotKeyCycleDisplaySize,
    /** Cycles the screen orientation through the specified rotations for the particular
	size mode you are in.
	For example, phones that support screen flipping may
	use this feature for changing between right and left handed use.
	For rectangular display modes you can only specify 2 orientations, 180 degrees
	apart. For square modes you can specify 4 rotations (90 degrees) or however
	many you want.
	Specification of the list of rotations takes place in the WSINI.INI file.
	Default key mapping: debug : \<Ctrl\>\<Alt\>\<Shift\> O. Release : none. */
    EHotKeyCycleOrientation,
    /** Increases the screen's brightness.
	The brightness wraps around to the minimum
	value after it has reached the maximum. */
    EHotKeyIncBrightness,
    /** Decreases the screen's brightness.
	The brightness wraps around to the maximum value after it has reached the minimum. */
    EHotKeyDecBrightness,

    /** Cycle focus screen from one to another in multiple screen environment. Start
	from current focused screen, switch to next the screen, and wraps around when it
	reaches the last screen.
	Default key mapping: \<Ctrl\>\<Alt\>\<Shift\> I. */
    EHotKeyCycleFocusScreen,

    /** Value for first hot key.
	Used with EHotKeyLastKeyType to make it easy to write a for loop that steps
	through all the different key values. */
    EHotKeyFirstKeyType = EHotKeyEnableLogging, //Must always be set to the first one
    /** Value for last hot key.
	Used with EHotKeyFirstKeyType to make it easy to write a for loop that steps
	through all the different key values. */
    EHotKeyLastKeyType = EHotKeyCycleFocusScreen, //Must always be set to the last one
};