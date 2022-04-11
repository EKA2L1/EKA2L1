/*
 * Copyright (c) 1994-2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project / Symbian Source Code
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

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>

#include <common/e32inc.h>
#include <common/vecx.h>
#include <common/types.h>

#include <drivers/graphics/emu_window.h>
#include <services/window/keys.h>

#include <utils/consts.h>
#include <utils/des.h>

enum {
    cmd_slot = 0,
    reply_slot = 1,
    remote_slot = 2
};

namespace eka2l1::epoc {
    enum {
        base_handle = 0x40000000
    };

    enum class graphics_orientation {
        normal,
        rotated90,
        rotated180,
        rotated270
    };

    /**
     * \brief Screen display mode.
     *
     * Depend on the display mode, the bitmap sends it will have the specified attribute.
    */
    enum class display_mode {
        none,
        gray2, ///< Monochrome display mode (1 bpp)
        gray4, ///< Four grayscales display mode (2 bpp)
        gray16, ///< 16 grayscales display mode (4 bpp)
        gray256, ///< 256 grayscales display mode (8 bpp)
        color16, ///< Low colour EGA 16 colour display mode (4 bpp)
        color256, ///< 256 colour display mode (8 bpp)
        color64k, ///< 64,000 colour display mode (16 bpp)
        color16m, ///< True colour display mode (24 bpp)
        rgb, ///< (Not an actual display mode used for moving buffers containing bitmaps)
        color4k, ///< 4096 colour display (12 bpp).
        color16mu, ///< True colour display mode (32 bpp, but top byte is unused and unspecified)
        color16ma, ///< Display mode with alpha (24bpp colour plus 8bpp alpha)
        color16map, ///< Pre-multiplied Alpha display mode (24bpp color multiplied with the alpha channel value, plus 8bpp alpha)
        color_last
    };

    int get_num_colors_from_display_mode(const display_mode disp_mode);
    bool is_display_mode_color(const display_mode disp_mode);
    bool is_display_mode_mono(const display_mode disp_mode);
    bool is_display_mode_alpha(const display_mode disp_mode);
    int get_bpp_from_display_mode(const epoc::display_mode bpp);
    int get_byte_width(const std::uint32_t pixels_width, const std::uint8_t bits_per_pixel);
    epoc::display_mode string_to_display_mode(const std::string &disp_str);
    std::string display_mode_to_string(const epoc::display_mode disp_mode);
    epoc::display_mode get_display_mode_from_bpp(const int bpp, const bool has_color);

    enum class pointer_cursor_mode {
        none, ///< The device don't have a pointer (touch)
        fixed, ///< Use the default system cursor
        normal, ///< Can't understand yet
        window, ///< Can't understand yet
    };

    enum class window_type {
        redraw,
        backed_up,
        blank
    };

    enum event_modifier {
        event_modifier_repeatable = 0x001,
        event_modifier_keypad = 0x002,
        event_modifier_left_alt = 0x004,
        event_modifier_right_alt = 0x008,
        event_modifier_alt = 0x010,
        event_modifier_left_ctrl = 0x020,
        event_modifier_right_ctrl = 0x040,
        event_modifier_ctrl = 0x080,
        event_modifier_left_shift = 0x100,
        event_modifier_right_shift = 0x200,
        event_modifier_shift = 0x400,
        event_modifier_left_func = 0x800,
        event_modifier_right_func = 0x1000,
        event_modifier_func = 0x2000,
        event_modifier_caps_lock = 0x4000,
        event_modifier_num_lock = 0x8000,
        event_modifier_scroll_lock = 0x10000,
        event_modifier_key_up = 0x20000,
        event_modifier_special = 0x40000,
        event_modifier_double_click = 0x80000,
        event_modifier_pure_key_code = 0x100000,
        event_modifier_cancel_rot = 0x200000,
        event_modifier_no_rot = 0x0,
        event_modifier_rotate90 = 0x400000,
        event_modifier_rotate180 = 0x800000,
        event_modifier_rotate270 = 0x1000000,
        event_modifier_adv_pointer = 0x10000000,
        event_modifier_all_mods = 0x1FFFFFFF
    };

    enum class event_type {
        /**
         * Button 1 or pen
         */
        button1down,
        button1up,
        /**
         * Middle button of a 3 button mouse
         */
        button2down,
        button2up,
        button3down,
        button3up,

        /**
         * Received when button 1 is down
         */
        drag,

        /**
         * Received when button 1 is up and the XY input mode is not open
         */
        move,
        button_repeat,
        switch_on,
        out_of_range,
        enter_close_proximity,
        exit_close_proximity,
        enter_high_pressure,
        exit_high_pressure,
        null_type = -1
    };

    enum class event_code {
        /** Null event.
        This can be sent, but should be ignored by clients. */
        null,

        /** Key event.
        This is the event that is sent when a character has been received from the
        keyboard.

        If an EEventKey event is associated with an EEventKeyDown or EEventKeyUp
        event (typically EEventKeyDown), the EEventKey event occurs after the
        EEventKeyDown/EEventKeyUp event.

        In practice, the only keys potentially likely to have their EEventKey event
        generated on the up rather than the down are modifier keys. */
        key,

        /** Key up event.
         
        If an EEventKey event is associated with an EEventKeyUp event (which is
        rarely the case), the EEventKey event occurs after the EEventKeyUp event. */
        key_up,

        /** Key down event.
        
        If an EEventKey event is associated with an EEventKeyDown event (which
        is typically the case), the EEventKey event occurs after the EEventKeyDown event. */
        key_down,

        /** Modifier changed event.
        
        This is an event generated by the window server when
        the state of one of the modifier keys changes.
        It is not reported unless explicitly requested by a window.
        @see RWindowTreeNode::EnableModifierChangedEvents(). */
        modifier_change,

        /** Pointer event.
        
        This event is sent when the user presses or releases a pointer button (or
        the equivalent action, depending on the type of pointing device), drags the
        pointer, moves it or uses the pointer to switch on the device. */
        touch,

        /** Pointer enter event.
	    This occurs when the user moves the pointer into a window with a pointer button
        pressed (or equivalent action depending on the type of pointing device). If
        move events are being generated, this event also occurs when the user moves
        the pointer into the window. */
        touch_enter,

        /** Pointer exit event.
        Occurs when the user moves the pointer out of a window with a pointer button
        pressed (or equivalent action depending on the type of pointing device). If
        move events are being generated, this event also occurs when the user moves
        the pointer out of the window. */
        touch_exit,

        /** Pointer move buffer ready event.
        Occurs when the pointer move buffer is ready to be retrieved by the client. */
        event_pointer_buffer_ready,

        /** Drag and drop */
        drag_and_drop,

        /** Focus lost event.
        Occurs when a window group loses keyboard focus. */
        focus_lost, //10

        /** Focus gained event.
        Occurs when a window group gains keyboard focus. */
        focus_gained,

        /** On event.
        This event type is not reported unless explicitly requested by a window.
        @see RWindowTreeNode::EnableOnEvents(). */
        switch_on,

        /** Password event.
        Occurs when the window server enters password mode. It is sent to the group
        window of the currently active password window.
        This is the window server mode where the user is required to enter a password
        before any further actions can be performed.
        @deprecated	*/
        event_password,

        /** Window group changed event. This occurs whenever a window group is destroyed,
        and whenever a window group's name changes
        This event type is not reported unless explicitly requested by a window.
        @see RWindowTreeNode::EnableGroupChangeEvents(). */
        window_groups_changed,

        /** Error event.
        Occurs when an error occurs. See TWsErrorMessage::TErrorCategory for the types
        of errors.
        This event type is not reported unless explicitly requested by a window.
        @see RWindowTreeNode::EnableErrorMessages(). */
        event_error_msg, //15

        /** Message ready event.
        Occurs when a session sends a message to this window group using RWsSession::SendMessageToWindowGroup(). */
        event_messages_ready,

        invalid, // For internal use only

        /** Off event.
        This is issued when an off event is received by the window server from the
        base.
        If for some reason the event can't be delivered, or there is no-one to deliver
        it to, then a call to the base is made to power down the processor.
        This event is only delivered if explicitly requested using RWsSession:RequestOffEvent(). */
        switch_off,

        /** Event issued to off-event requesting windows when the off key is pressed. */
        key_switch_off,

        /** Screen size mode change event.
        This is issued when the screen size mode has changed, for instance when
        the cover on a phone that supports screen flipping is opened or closed. */
        screen_change, //20

        /** Event sent whenever the window group with focus changes.
        Requested by RWindowTreeNode::EnableFocusChangeEvents(). */
        focus_group_changed,

        /** Case opened event.
        This event is sent to those windows that have requested EEventSwitchOn
        events. Unlike with EEventSwitchOn events, the screen will not be switched
        on first. */
        case_opened,

        /** Case closed event.
        This event is sent to those windows that have requested EEventSwitchOff
        events.
        Unlike EEventSwitchOff events, which make a call to the base to power down
        the processor if for some reason the event can't be delivered (or there is
        no-one to deliver it to), failure to deliver case closed events has no repercussions. */
        case_closed,

        /** Window group list change event.
        The window group list is a list of all window groups and their z-order. This
        event indicates any change in the window group list: additions, removals and
        reorderings.
        Notification of this event is requested by calling RWindowTreeNode::EnableGroupListChangeEvents(). */
        group_list_change,

        /** The visibility of a window has changed
        This is sent to windows when they change from visible to invisible, or visa versa, usually due
        to another window obscuring them.
        Notification of this event is requested by calling RWindowTreeNode::EnableVisibilityChangeEvents(). */
        window_visibility_change,

#ifdef SYMBIAN_PROCESS_MONITORING_AND_STARTUP
        /** Restart event.

        This is issued when an restart event is received by the window server from the 
        base. This event is also an off event, because it might power-cycle the device.

        If for some reason the event can't be delivered, or there is no-one to deliver 
        it to, then a call to the base is made to power down the processor.

        This event is only delivered if explicitly requested using RWsSession:RequestOffEvent(). */
        restart_system,
#endif

        /** The display state or configuration has changed

        Either change of the current resolution list (state change) or current resolution/background
        (mode change) will trigger this event.

        Notification of this event is requested by calling MDisplayControl::EnableDisplayChangeEvents() 
            */
        display_changed = window_visibility_change + 2,

        //Codes for events only passed into Key Click DLL's
        /** Repeating key event.
        This is only sent to a key click plug-in DLL (if one is present) to indicate
        a repeating key event.
        @see CClickMaker */
        key_repeat = 100,

        group_win_open,
        group_win_close,

        win_close,

        //Codes for events only passed into anim dlls
        /** Direct screen access begin
        This is only sent to anim dlls (if they register to be notified). It indicates that
        the number of direct screen access sessions has increased from zero to one.*/
        direct_access_begin = 200,

        /** Direct screen access end
        This is only sent to anim dlls (if they register to be notified). It indicates that
        the number of direct screen access sessions has decreased from one to zero.*/
        direct_access_end,
        /** Event to signal the starting or stopping of the wserv heartbeat timer
        This is only sent to anim dlls (if they register to be notified). */
        heartbeat_timer_changed,

        //The range 900-999 is reserved for UI Framework events
        /** 900-909 WSERV protects with PowerMgmt */
        power_mgmt = 900,
        reserved = 910,

        //Event codes from EEventUser upwards may be used for non-wserv events.
        //No event codes below this should be defined except by the window server

        /** User defined event.
        The client can use this and all higher values to define their own
        events. These events can be sent between windows of the same client or windows
        of different clients.
        @see RWs::SendEventToWindowGroup(). */
        user = 1000,
    };

    struct key_event {
        std::uint32_t code;
        std::int32_t scancode;
        std::uint32_t modifiers;
        std::int32_t repeats;
    };

    struct pointer_event {
        event_type evtype;
        std::uint32_t modifier = 0;
        eka2l1::vec2 pos;
        eka2l1::vec2 parent_pos;
    };

    struct message_ready_event {
        std::int32_t window_group_id;
        epoc::uid message_uid;
        std::int32_t message_parameters_size;
    };

    struct window_visiblity_changed_event {
        enum {
            partially_visible = 1 << 0,
            not_visible = 1 << 1,
            fully_visible = 1 << 2
        };

        std::uint32_t flags_;
    };

    struct adv_pointer_event : public pointer_event {
        int pos_z;
        std::uint8_t ptr_num; // multi touch
        std::uint8_t padding[3];
    };

    enum pointer_filter_type {
        pointer_none = 0x00,
        pointer_enter = 0x01, ///< In/out
        pointer_move = 0x02,
        pointer_drag = 0x04,
        pointer_simulated_event = 0x08,
        all = pointer_move | pointer_simulated_event
    };

    enum class text_alignment {
        left,
        center,
        right
    };

    enum class event_control {
        always,
        only_with_keyboard_focus,
        only_when_visible
    };

    struct event {
        event_code type;
        std::uint32_t handle;
        std::uint64_t time;

        // TODO: Should be only pointer event with epoc < 9.
        // For epoc9 there shouldnt be a pointer number, since there is no multi touch
        union {
            adv_pointer_event adv_pointer_evt_;
            key_event key_evt_;
            message_ready_event msg_ready_evt_;
            window_visiblity_changed_event win_visibility_change_evt_;
        };

        event() {}
        event(const std::uint32_t handle, event_code evt_code);

        void operator=(const event &rhs) {
            type = rhs.type;
            handle = rhs.handle;
            time = rhs.time;

            adv_pointer_evt_ = rhs.adv_pointer_evt_;
        }
    };

    struct redraw_event {
        std::uint32_t handle;
        vec2 top_left;
        vec2 bottom_right;
    };

    static_assert(sizeof(redraw_event) == 20);

    constexpr key_code keymap[] = {
        key_null,
        key_backspace,
        key_tab,
        key_enter,
        key_escape,
        key_space,
        key_print_screen,
        key_pause,
        key_home,
        key_end,
        key_page_up,
        key_page_down,
        key_insert,
        key_delete,
        key_left_arrow,
        key_right_arrow,
        key_up_arrow,
        key_down_arrow,
        key_left_shift,
        key_right_shift,
        key_left_alt,
        key_right_alt,
        key_left_ctrl,
        key_right_ctrl,
        key_left_func,
        key_right_func,
        key_caps_lock,
        key_num_lock,
        key_scroll_lock,
        key_f1,
        key_f2,
        key_f3,
        key_f4,
        key_f5,
        key_f6,
        key_f7,
        key_f8,
        key_f9,
        key_f10,
        key_f11,
        key_f12,
        key_f13,
        key_f14,
        key_f15,
        key_f16,
        key_f17,
        key_f18,
        key_f19,
        key_f20,
        key_f21,
        key_f22,
        key_f23,
        key_f24,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_null,
        key_menu,
        key_backlight_on,
        key_backlight_off,
        key_backlight_toggle,
        key_inc_contrast,
        key_dec_contrast,
        key_slider_down,
        key_slider_up,
        key_dictaphone_play,
        key_dictaphone_stop,
        key_dictaphone_record,
        key_help,
        key_off,
        key_dial,
        key_inc_volume,
        key_dec_volume,
        key_device_0,
        key_device_1,
        key_device_2,
        key_device_3,
        key_device_4,
        key_device_5,
        key_device_6,
        key_device_7,
        key_device_8,
        key_device_9,
        key_device_a,
        key_device_b,
        key_device_c,
        key_device_d,
        key_device_e,
        key_device_f,
        key_application_0,
        key_application_1,
        key_application_2,
        key_application_3,
        key_application_4,
        key_application_5,
        key_application_6,
        key_application_7,
        key_application_8,
        key_application_9,
        key_application_a,
        key_application_b,
        key_application_c,
        key_application_d,
        key_application_e,
        key_application_f,
        key_yes,
        key_no,
        key_inc_brightness,
        key_dec_brightness,
        key_keyboard_extend,
        key_device_10,
        key_device_11,
        key_device_12,
        key_device_13,
        key_device_14,
        key_device_15,
        key_device_16,
        key_device_17,
        key_device_18,
        key_device_19,
        key_device_1a,
        key_device_1b,
        key_device_1c,
        key_device_1d,
        key_device_1e,
        key_device_1f,
        key_application_10,
        key_application_11,
        key_application_12,
        key_application_13,
        key_application_14,
        key_application_15,
        key_application_16,
        key_application_17,
        key_application_18,
        key_application_19,
        key_application_1a,
        key_application_1b,
        key_application_1c,
        key_application_1d,
        key_application_1e,
        key_application_1f,
        key_device_20,
        key_device_21,
        key_device_22,
        key_device_23,
        key_device_24,
        key_device_25,
        key_device_26,
        key_device_27,
        key_application_20,
        key_application_21,
        key_application_22,
        key_application_23,
        key_application_24,
        key_application_25,
        key_application_26,
        key_application_27
    };

    enum dsa_terminate_reason {
        dsa_terminate_cancel = 0,
        dsa_terminate_region = 1,
        dsa_terminate_screen_display_mode_change = 2,
        dsa_terminate_rotation_change = 3
    };

    static constexpr std::uint8_t WS_MAJOR_VER = 1;
    static constexpr std::uint8_t WS_MINOR_VER = 0;
    static constexpr std::uint16_t WS_OLDARCH_VER = 139;
    static constexpr std::uint16_t WS_NEWARCH_VER = 151;
    static constexpr std::uint64_t WS_DEFAULT_KEYBOARD_REPEAT_INIT_DELAY = 300000;
    static constexpr std::uint64_t WS_DEFAULT_KEYBOARD_REPEAT_NEXT_DELAY = 100000;

    key_code map_scancode_to_keycode(std_scan_code scan_code);
    std_scan_code post_processing_scancode(std_scan_code input_code, int ui_rotation);

    static constexpr std::uint32_t KEYBIND_TYPE_MOUSE_CODE_BASE = 0x10000000;

    typedef std::map<std::pair<int, int>, std::uint32_t> button_map;
    typedef std::map<std::uint32_t, std::uint32_t> key_map;

    std::optional<std::uint32_t> map_button_to_inputcode(button_map &map, int controller_id, int button);
    std::optional<std::uint32_t> map_key_to_inputcode(key_map &map, std::uint32_t keycode);

    // TODO: This should not depends on the version of the system at all. This function is basically stereotyping
    int get_approximate_pixel_to_twips_mul(const epocver ver);
}
