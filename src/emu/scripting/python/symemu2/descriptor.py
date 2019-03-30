# Copyright (c) 2018 EKA2L1 Team.
# 
# This file is part of EKA2L1 project 
# (see bentokun.github.com/EKA2L1).
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

import ctypes
import symemu

from enum import Enum

import struct

class DescriptorType(Enum):
    BUF_CONST = 0
    PTR_CONST = 1
    PTR =  2
    BUF = 3
    BUF_CONST_PTR = 4

# Provide read access to a in-memory descriptor
class DescriptorBase(object):
    def __init__(self, address):
        def get_ptr_buf_const():
            return address + 4
            
        def get_ptr_ptr_const():
            return symemu.readDword(address + 4)
            
        def get_ptr_buf():
            return address + 8
            
        def get_ptr_ptr():
            return symemu.readWord(address + 8)
            
        def get_ptr_buf_const_ptr():
            real_buf_addr = symemu.readWord(address + 8)
            return real_buf_addr + 4
        
        ptr_switcher = {
            DescriptorType.BUF_CONST: get_ptr_buf_const,
            DescriptorType.PTR_CONST: get_ptr_ptr_const,
            DescriptorType.PTR: get_ptr_ptr,
            DescriptorType.BUF: get_ptr_buf,
            DescriptorType.BUF_CONST_PTR: get_ptr_buf_const_ptr
        }
        
        lengthAndType = symemu.readDword(address)
        self.length = lengthAndType & 0xFFFFFF
        self.type = DescriptorType(lengthAndType >> 28)
        self.ptr = ptr_switcher.get(self.type, lambda: 'Invalid descriptor type')()
        
class Descriptor8(DescriptorBase):
    def __init__(self, address):
        DescriptorBase.__init__(self, address)

    def __str__(self):
        retstr = ''
    
        for i in range(0, self.length - 1):
            c = symemu.readByte(self.ptr + i * 1)
            retstr += struct.pack('<C', c).decode('utf-8')

        return retstr                    
        
class Descriptor16(DescriptorBase):
    def __init__(self, address):
        DescriptorBase.__init__(self, address)

    def __str__(self):
        retstr = u''
        
        for i in range(0, self.length - 1):
            uc = symemu.readWord(self.ptr + i * 2)
            retstr += struct.pack('<H', uc).decode('utf-16')

        return retstr
