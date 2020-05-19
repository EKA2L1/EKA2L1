# Copyright (c) 2020 EKA2L1 Team.
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

import struct
import symemu
import symemu2.events
import symemu2.utils.string as StringUtils


# Hook that invokes when message opcode 0, to the server named "!AknIconServer" is sent.
# The opcode 0 is RetrieveOrCreateSharedIcon
@symemu2.events.emulatorIpcInvoke('!AknIconServer', 0)
def retrieveGetSendHook(ctx):
    params = ctx.getArgument(0).rawData()

    # First field of the param struct is a static UCS2 descriptor.
    # That's the name of the file containg icon pool.
    (fileNameMaxLen, filename) = StringUtils.getStaticUcs2String(params)
    offsetStart = 8 + fileNameMaxLen * 2

    # Extract the bitmap ID and mask ID. These all takes 4 bytes each
    (bitmapId, maskId) = struct.unpack('<ll', params[offsetStart: offsetStart + 8])
    symemu.emulog('From file {}, bitmap ID {}, mask ID {}', filename, bitmapId, maskId)


# Hook that invokes when a message with opcode 0, from "!AknIconServer" yeilds complete.
@symemu2.events.emulatorIpcInvoke('!AknIconServer', 0, symemu2.events.IpcInvokementType.COMPLETE)
def retrieveGetCompleteHook(ctx):
    returnParams = ctx.getArgument(1).rawData()

    # First 4-byte contains source bitmap handle, while the next contains mask bitmap handle.
    (bitmapHandle, maskHandle) = struct.unpack('<LL', returnParams[:8])

    symemu.emulog('Rendered icon to bitmap handle {}, mask handle {}', bitmapHandle, maskHandle)
