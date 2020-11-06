/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include <gdbstub/gdbstub.h>

#include <common/configure.h>
#include <common/cvt.h>
#include <common/ini.h>
#include <common/log.h>
#include <common/path.h>
#include <common/platform.h>
#include <common/types.h>
#include <console/state.h>
#include <console/thread.h>
#include <debugger/imgui_debugger.h>
#include <debugger/logger.h>
#include <drivers/audio/audio.h>
#include <drivers/graphics/emu_window.h>
#include <drivers/graphics/graphics.h> // Declaration for graphics driver. Happy!
#include <system/epoc.h>
#include <loader/rom.h>
#include <config/config.h>

#include <kernel/libmanager.h>

#if EKA2L1_PLATFORM(WIN32) && defined(_MSC_VER) && ENABLE_SEH_HANDLER
#include <console/seh_handler.h>
#include <eh.h>
#endif

#include <imgui.h>
#include <yaml-cpp/yaml.h>

#if defined(_MSC_VER)
#define EKA2L1_EXPORT __declspec(dllexport)
#else
#define EKA2L1_EXPORT __attribute__((dllexport))
#endif

extern "C" {
EKA2L1_EXPORT uint32_t NvOptimusEnablement = 0x00000001;
EKA2L1_EXPORT uint32_t AmdPowerXpressRequestHighPerformance = 0x00000001;
}

using namespace eka2l1;

int main(const int argc, const char **argv) {
    // Episode 1: I think of a funny joke about Symbian and I will told my virtual wife
    std::cout << "-------------- EKA2L1: Experimental Symbian Emulator -----------------"
              << std::endl;

    // Change current directory to executable directory, prevent any confusion
    const auto executable_directory = eka2l1::file_directory(argv[0]);
    eka2l1::set_current_directory(executable_directory);

    std::unique_ptr<eka2l1::desktop::emulator> state = std::make_unique<eka2l1::desktop::emulator>();
    int result = emulator_entry(*state, argc, argv);

    // We can do memory leak check here. If anyone ever wants
    //_CrtDumpMemoryLeaks();
    return result;
}
