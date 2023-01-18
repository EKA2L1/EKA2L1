/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#ifndef OPTION
#define OPTION(name, variable, default_var)
#endif

#ifndef DEFAULT_IMI
#define DEFAULT_IMI "540806859904945"
#endif

#ifndef DEFAULT_MMC_ID
#define DEFAULT_MMC_ID "00000000-00000000-00000000-00000000"
#endif

OPTION(bkg-path, bkg_path, "")
OPTION(font, font_path, "")
OPTION(log-read, log_read, false)
OPTION(log-write, log_write, false)
OPTION(log-ipc, log_ipc, false)
OPTION(log-svc, log_svc, false)
OPTION(log-passed, log_passed, false)
OPTION(log-exports, log_exports, false)
OPTION(cpu, cpu_backend, "dynarmic")
OPTION(device, device, 0)
OPTION(language, language, -1)
OPTION(emulator-language, emulator_language, -1)
OPTION(enable-gdb-stub, enable_gdbstub, false)
OPTION(data-storage, storage, "data")
OPTION(gdb-port, gdb_port, 24689)
OPTION(internet-bluetooth-port, internet_bluetooth_port, 35689)
OPTION(enable-srv-rights, enable_srv_rights, true)
OPTION(enable-srv-sa, enable_srv_sa, true)
OPTION(enable-srv-drm, enable_srv_drm, true)
OPTION(fbs-enable-compression-queue, fbs_enable_compression_queue, false)
OPTION(enable-btrace, enable_btrace, false)
OPTION(stop-warn-touchscreen-disabled, stop_warn_touch_disabled, false)
OPTION(dump-imb-range-code, dump_imb_range_code, false)
OPTION(hide-mouse-in-screen-space, hide_mouse_in_screen_space, false)
OPTION(enable-nearest-neighbor-filter, nearest_neighbor_filtering, true)
OPTION(integer-scaling, integer_scaling, true)
OPTION(cpu-load-save, cpu_load_save, true)
OPTION(mime-detection, mime_detection, true)
OPTION(rtos-level, rtos_level, "mid")
OPTION(ui-new-style, ui_new_style, true)
OPTION(svg-icon-cache-reset, svg_icon_cache_reset, false)
OPTION(imei, imei, DEFAULT_IMI)
OPTION(mmc-id, mmc_id, DEFAULT_MMC_ID)
OPTION(audio-master-volume, audio_master_volume, 100)
OPTION(current-keybind-profile, current_keybind_profile, "default")
OPTION(screen-buffer-sync, screen_buffer_sync_string, "preferred")
OPTION(report-mmfdev-underflow, report_mmfdev_underflow, false)
OPTION(disable-display-content-scale, disable_display_content_scale, false)
OPTION(device-display-name, device_display_name, "EKA2L1")
OPTION(midi-backend, midi_backend_string, "tsf")
OPTION(hsb-bank-path, hsb_bank_path, "resources/defaultbank.hsb")
OPTION(sf2-bank-path, sf2_bank_path, "resources/defaultbank.sf2")
OPTION(bt-central-server-url, bt_central_server_url, "btnetplay.12z1.com")
OPTION(enable-hw-gles1, enable_hw_gles1, true)
OPTION(log-filter, log_filter, DEFAULT_LOG_FILTERING)
OPTION(hide-system-apps, hide_system_apps, true)
OPTION(btnet-port-offset, btnet_port_offset, 15000)
OPTION(btnet-password, btnet_password, "")
OPTION(btnet-discovery-mode, btnet_discovery_mode, 0)
OPTION(enable-upnp, enable_upnp, true)
OPTION(extensive-logging, extensive_logging, false)

#ifdef OPTION
#undef OPTION
#endif