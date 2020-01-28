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

def getStaticConstStringImpl(bytes, sizeOfComp, format):
    length = (struct.unpack('<L', bytes[:4])) & 0xFFFFFF
    (str, ) = struct.unpack('<{}s'.format(length * sizeOfComp), bytes[4:4 + length * sizeofComp])
    return str.decode(format)

def getStaticStringImpl(bytes, sizeofComp, format):
    (length, maxLength) = struct.unpack('<LL', bytes[:8])
    length &= 0xFFFFFF
    (str, ) = struct.unpack('<{}s'.format(length * sizeofComp), bytes[8:8 + length * sizeofComp])
    return (maxLength, str.decode(format))
    
# Get constant UTF-8 descriptor (string) from array of bytes.
#
# A constant UTF-8 descriptor has first 4 bytes as length, and the following
# bytes containg the content.
#
# Parameter
# -------------------------
# bytes:    bytes
#   Array of bytes to retrieve the string from
#
# Returns
# -------------------------
# string
#   UTF-8 string on success.
def getStaticConstUtf8String(bytes):
    return getStaticConstStringImpl(bytes, 1, 'utf-8')

# Get constant UTF-16 descriptor (string) from array of bytes.
#
# A constant UTF-16 descriptor has first 4 bytes as length, and the following
# bytes containg the content.
#
# Parameter
# -------------------------
# bytes:    bytes
#   Array of bytes to retrieve the string from
#
# Returns
# -------------------------
# string
#   UTF-16 string on success.    
def getStaticConstUcs2String(bytes):
    return getStaticConstStringImpl(bytes, 2, 'utf-16')

# Get modifiable UTF-8 descriptor (string) from array of bytes.
#
# An UTF-8 descriptor has first 4 bytes as length, next 4 bytes as
# max length, and the following bytes containg the content.
#
# Parameter
# -------------------------
# bytes:    bytes
#   Array of bytes to retrieve the string from
#
# Returns
# -------------------------
# string
#   UTF-8 string on success.    
def getStaticUtf8String(bytes):
    return getStaticStringImpl(bytes, 1, 'utf-8')
    
# Get modifiable UCS2 descriptor (string) from array of bytes.
#
# An UCS2 descriptor has first 4 bytes as length, next 4 bytes as
# max length, and the following bytes containg the content.
#
# Parameter
# -------------------------
# bytes:    bytes
#   Array of bytes to retrieve the string from
#
# Returns
# -------------------------
# string
#   UCS2 string on success.    
def getStaticUcs2String(bytes):
    return getStaticStringImpl(bytes, 2, 'utf-16')

