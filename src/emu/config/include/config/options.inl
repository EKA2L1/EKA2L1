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

OPTION(ui-scale, ui_scale, 1.0f)
OPTION(bkg-alpha, bkg_transparency, 129)
OPTION(bkg-path, bkg_path, "")
OPTION(font, font_path, "")
OPTION(log-read, log_read, false)
OPTION(log-write, log_write, false)
OPTION(log-ipc, log_ipc, false)
OPTION(log-svc, log_svc, false)
OPTION(log-passed, log_passed, false)
OPTION(log-exports, log_exports, false)
OPTION(cpu, cpu_backend, 0)
OPTION(device, device, 0)
OPTION(language, language, -1)
OPTION(emulator-language, emulator_language, -1)
OPTION(enable-gdb-stub, enable_gdbstub, false)
OPTION(data-storage, storage, "")
OPTION(gdb-port, gdb_port, 24689)
OPTION(enable-srv-ecom, enable_srv_ecom, true)
OPTION(enable-srv-cenrep, enable_srv_cenrep, true)
OPTION(enable-srv-backup, enable_srv_backup, true)
OPTION(enable-srv-install, enable_srv_install, true)
OPTION(enable-srv-rights, enable_srv_rights, true)
OPTION(enable-srv-sa, enable_srv_sa, true)
OPTION(enable-srv-drm, enable_srv_drm, true)
OPTION(enable-srv-eikappui, enable_srv_eikapp_ui, true)
OPTION(enable-srv-akn-icon, enable_srv_akn_icon, true)
OPTION(enable-srv-akn-skin, enable_srv_akn_skin, true)
OPTION(enable-srv-cdl, enable_srv_cdl, true)
OPTION(enable-srv-socket, enable_srv_socket, false)
OPTION(fbs-enable-compression-queue, fbs_enable_compression_queue, false)
OPTION(accurate-ipc-timing, accurate_ipc_timing, false)
OPTION(enable-btrace, enable_btrace, false)
OPTION(stop-warn-touchscreen-disabled, stop_warn_touch_disabled, false)
OPTION(dump-imb-range-code, dump_imb_range_code, false)
OPTION(hide-mouse-in-screen-space, hide_mouse_in_screen_space, false)
OPTION(enable-nearest-neighbor-filter, nearest_neighbor_filtering, false)
OPTION(integer-scaling, integer_scaling, true)
OPTION(cpu-load-save, cpu_load_save, true)
OPTION(rtos-level, rtos_level, "mid")

#ifdef OPTION
#undef OPTION
#endif