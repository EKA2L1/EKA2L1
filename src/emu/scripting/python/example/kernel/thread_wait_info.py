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
import symemu2.events
import symemu2.svc

@symemu2.events.emulatorSystemCallInvoke(symemu2.svc.Epoc9Svc.WaitForAnyRequest)
def waitForRequestWhoHook():
    # Get current thread
    crrThread = symemu.getCurrentThread()
    symemu.emulog('Thread {} will wait for any request!'.format(crrThread.getName()))
