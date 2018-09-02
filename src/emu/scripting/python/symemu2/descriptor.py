import ctypes
import symemu

from enum import Enum

class DescriptorType(Enum):
    BUF_CONST = 0,
    PTR_CONST = 1,
    PTR =  2,
    BUF = 3,
    BUF_CONST_PTR = 4

# Provide read access to a in-memory descriptor
class DescriptorBase(object):
    def __init__(self, address):
        def get_ptr_buf_const():
            return address + 4
            
        def get_ptr_ptr_const():
            return symemu.readWord(address + 4)
            
        def get_ptr_buf():
            return address + 8
            
        def get_ptr_ptr():
            return symemu.readWord(address + 8)
            
        def get_ptr_buf_const_ptr():
            real_buf_addr = symemu.readWord(address + 8)
            return real_buf_addr + 4
        
        ptr_switcher = {
            BUF_CONST: get_ptr_buf_const,
            PTR_CONST: get_ptr_ptr_const,
            PTR: get_ptr_ptr,
            BUF: get_ptr_buf,
            BUF_CONST_PTR: get_ptr_buf_const_ptr
        };
        
        lengthAndType = symemu.readWord(address)
        self.length = lengthAndType & 0xFFFFFF
        self.type = DescriptorType(lengthAndType >> 28)
        
        self.ptr = ptr_switcher.get(self.type, lambda: 'Invalid descriptor type')()
        
    def type(self):
        return self.type
        
    def length(self):
        return self.length
        
class Descriptor8(DescriptorBase):
    def __init__(self, address):
        DescriptorBase.init(self, address)

    def __str__(self):
        str = '';
    
        for i in range(0, self.length - 1):
            str += symemu.readByte(self.ptr + i * 1).decode('utf-8')

        return str                    
        
class Descriptor16(DescriptorBase):
    def __init__(self, address):
        DescriptorBase.init(self, address)

    def __str__(self):
        str = u'';
    
        for i in range(0, self.length - 1):
            str += symemu.readWord(self.ptr + i * 2).decode('utf-16')

        return str