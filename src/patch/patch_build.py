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

import build
import os
import sys

def read_target_info(common_path):
    target_path = os.path.join(common_path, 'target.inf')
    
    try:
        list = []
        line_count = 0
        
        with open(target_path, 'r') as target_file:
            while True:
                line = target_file.readline()
                
                if line == '':
                    return list
                
                line_count += 1
                target_info = line.split()
                if len(target_info) < 2:
                    if len(target_info) == 1:
                        list.append((None, target_info[0]))
                    else:
                        print('Line {}: Not enough target info'.format(line_count))
                    
                    continue
                
                
                list.append((target_info[0], target_info[1]))
    except:
        print('Fail to open target info file!')
        return []

def main():
    root_path = sys.argv[len(sys.argv) - 1]
    common_path = root_path

    targets = read_target_info(common_path)
    if targets == None:
        print('No target available. Abort')
        return 0
    
    for target in targets:
        (target_platform, sdk_name) = target
        target_path = root_path
        
        if target_platform != None:
            target_path = os.path.join(target_path, target_platform + '\\')
        
        argvs = ['stub', '--device', sdk_name, '--config', 'urel', '--result', '..', target_path]
        build.build_entry(argvs)
    
    return 0

if __name__ == '__main__':
    main()