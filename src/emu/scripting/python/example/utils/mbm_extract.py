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

from symemu2.common import MbmReader


def main():
    # Load the mbm file. Pass the path to the mbm file.
    mbmf = MbmReader('smile.mbm')

    # Print total of bitmap in this multi-bitmap file.
    print('Total bitmap count: {}'.format(mbmf.bitmapCount()))

    # Iterate through bitmaps.
    for i in range(0, mbmf.bitmapCount()):
        (dimx, dimy) = mbmf.dimension(i)

        # Print bitmap information.
        print('\nBitmap no {}, bits per pixel: {}, size: {}x{}'.format(i, mbmf.bitsPerPixel(i), dimx, dimy))

        # Saving bitmap.
        if mbmf.saveBitmap(i, 'smile_{}.bmp'.format(i)) != True:
            print('\tError in saving bitmap no {}'.format(i))
        else:
            print('\tSuccess in saving bitmap no {}'.format(i))


if __name__ == '__main__':
    main()
