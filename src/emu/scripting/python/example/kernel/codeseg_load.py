# Copyright (c) 2019 EKA2L1 Team.
# 
# This file is part of EKA2L1 project.
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

import symemu


def scriptEntry():
    # Load EUSER DLL.
    seg = symemu.loadCodeseg('euser.dll')

    # Print code runtime information
    symemu.emulog('Runtime code address: 0x{:X}, size: 0x{:X}'.format(seg.codeRunAddress(), seg.codeSize()))
    symemu.emulog('Runtime data address: 0x{:X}, size: 0x{:X}'.format(seg.dataRunAddress(), seg.dataSize()))
    symemu.emulog('Runtime bss address: 0x{:X}, size: 0x{:X}'.format(seg.bssRunAddress(), seg.bssSize()))
    symemu.emulog('Total exports: {}'.format(seg.exportCount()))
    symemu.emulog('Export 649 loaded address: 0x{:X}'.format(seg.lookup(649)))
