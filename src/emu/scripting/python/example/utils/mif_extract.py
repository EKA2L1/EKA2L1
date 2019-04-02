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

from symemu2.common import MifReader

def main():
    # Load the MIF file. Pass the path to the MIF file.
    miff = MifReader('sample.mif')

    # Print total of entry in this MIF file.
    print('Total entry count: {}'.format(miff.entryCount()))
    
    for i in range(0, miff.entryCount()):
      print('Entry {} data size: {}'.format(i, miff.entryDataSize(i)))

    # Ideally, we can get the data of the entry with:
    # buf = miff.readEntry(i)
    # This will returns a string with binary data you can pack and unpack.

if __name__ == '__main__':
    main()
