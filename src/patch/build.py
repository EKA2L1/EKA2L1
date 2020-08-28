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

import os
import platform

from xml.dom import minidom

import timeit
import subprocess
import sys

from shutil import copyfile


# Check if the OS is 64-bit
def is_os_64bit():
    return platform.machine().endswith('64')


# Check if a build configuration string is valid
def is_valid_build_config(config):
    return (config == 'udeb') or (config == 'urel')


# Find the common files folder that store devices.xml
def find_symbian_common_path():
    if os.name == 'nt':
        import winreg
    
        # Try to read register
        reg = winreg.ConnectRegistry(None, winreg.HKEY_LOCAL_MACHINE)
        
        try:
            key = winreg.OpenKey(reg, r'SOFTWARE\WOW6432Node\Symbian\EPOC SDKs')
        except WindowsError:
            # Try to open the key again but assuming it's 32-bit
            try:
                key = winreg.OpenKey(reg, r'SOFTWARE\Symbian\EPOC SDKs')
            except WindowsError:
                key = None
                
        # If the key is not found
        if key != None:
            return winreg.QueryValueEx(key, 'CommonPath')[0]
        
        # Returns the default path then
        if is_os_64bit():
            return 'C:\Program Files (x86)\Common Files\Symbian'
            
        return 'C:\Program Files\Common Files\Symbian'
    
    # If it's not NT, there's no specific method... Maybe just returns empty
    return ''


def parse_devices(common_path):
    xml_path = common_path + '\devices.xml'
    
    if not os.path.exists(xml_path):
        raise Exception("Can't find devices.xml!")

    devices = []

    doc = minidom.parse(xml_path)
    devices_xml = doc.getElementsByTagName('devices')[0]
    device_list = devices_xml.getElementsByTagName('device')

    for device in device_list:
        root = device.getElementsByTagName('epocroot')[0]
        dvc_id = device.getAttribute('id')
        name = device.getAttribute('name')
        is_default_text = device.getAttribute('default')
        is_default = (is_default_text.lower() == 'yes')

        devices.append(((dvc_id + ':' + name), root.firstChild.data, is_default))

    return devices

def set_default_device(common_path, target_device, old_device):
    (target_id, target_epocroot, tdc) = target_device
    (old_id, old_epocroot, odc) = old_device

    if target_id == old_id:
        return

    subprocess.call('devices -setdefault @' + target_id, stdout=subprocess.PIPE)

def get_default_device(devices):
    if len(devices) == 0:
        return None

    for device in devices:
        if device[2]:
            return device

    return devices[0]


def parse_system_arguments(common_path, argvs):
    if len(argvs) <= 1:
        return [None, None, None, None, False]

    argpointer = 1
    group_path = argvs[len(argvs) - 1]
    configuration = 'urel'

    try:
        devices = parse_devices(common_path)
    except Exception as e:
        print(e)
        return [None, None, None, None, None, False]

    device = get_default_device(devices)
    olddevice = device

    if not device:
        print('No device presented. Build aborted')
        return [None, None, None, None, None, False]

    build_result_folder = None

    while argpointer <= len(argvs) - 1:
        command = argvs[argpointer].lower()

        if command == '--list':
            for device in devices:
                print(device)

            return [None, None, group_path, None, None, False]
        elif command == '--device':
            argpointer += 1

            try:
                device_number = int(argvs[argpointer])

                if device_number >= 0:
                    # Other smaller than 0, we will assume they want to use default device.
                    set_default_device(common_path, devices[device_number], device)
                    device = devices[device_number]
            except:
                found = False

                for small_device in devices:
                    (small_dvc_name, small_dvc_ep, small_dvc_d) = small_device
                    if small_dvc_name == argvs[argpointer]:
                        set_default_device(common_path, small_device, device)
                        device = small_device
                        found = True

                        break

                if not found:
                    raise Exception('No device found with name: {}'.format(argvs[argpointer]))
        elif command == '--config':
            argpointer += 1
            configuration = argvs[argpointer]

            # Is it valid...
            if not is_valid_build_config(configuration):
                raise Exception('{} is not a valid build configuration!\n Valid build configurations: udeb, urel'.
                                format(configuration))
        elif command == '--result':
            argpointer += 1
            build_result_folder = argvs[argpointer]
        elif command == '--help':
            print('\t--help:    Display this command.')
            print('\t--result:  Specify the folder storing build result')
            print('\t--config:  Select build configuration.')
            print('\t--device:  Select the device ID to build the project.')
            print('\t--list:    List all available devices installed on this computer.')

            return [None, None, None, None, None, False]

        argpointer += 1

    return [device, olddevice, group_path, configuration, build_result_folder, True]


# Get the variant.cfg absolute path
# This config file is used to generate ABLD makefile for basic EPOC SDK path supplies.
def get_variant_config_file():
    return os.path.join(os.environ['EPOCROOT'], 'epoc32\\tools\\variant\\variant.cfg')

def get_sdk_is_eka1():
    return not os.path.exists(os.path.join(os.environ['EPOCROOT'], 'epoc32\\tools\\elf2e32.exe'))


# Get the folder that store a MMP's ABLD makefile and build results
def get_mmp_build_folder(mmp_full_path):
    build_path = os.path.join(os.environ['EPOCROOT'], 'epoc32\\BUILD\\')
    (drive, mmp_file_abs) = os.path.splitdrive(mmp_full_path)
    (build_folder_mmp, ext) = os.path.splitext(mmp_file_abs)
    build_folder_mmp += os.path.sep

    return os.path.join(drive, build_path, build_folder_mmp[1:])


# Check if this mmp should has its ABLD makefile generated
def should_generate_abld_makefile(mmp_full_path, plat):
    if get_sdk_is_eka1():
        return True

    mmp_build_folder = get_mmp_build_folder(mmp_full_path)
    (folder, mmp_filename) = os.path.split(mmp_full_path)
    (mmp_name, ext) = os.path.splitext(mmp_filename)

    abld_makefile_name = '{}\\{}.{}'.format(plat, mmp_name, plat).upper()
    abld_makefile_path = os.path.join(mmp_build_folder, abld_makefile_name)

    if not os.path.exists(abld_makefile_path):
        return True

    # Check the timestamp variant config file is modified
    variant_file_mtime = os.path.getmtime(get_variant_config_file())
    abld_makefile_mtime = os.path.getmtime(abld_makefile_path)

    return variant_file_mtime > abld_makefile_mtime


def invoke_abld_command(command):
    print('Invoke ABLD command {}'.format(command))
    result = subprocess.call(['abld.bat'] + command, stdout=subprocess.PIPE)

    if result != 0:
        raise Exception('ABLD command failed with code ', result)


# Parse bld.inf files to get list of target MMPs and the target build platform
def parse_bld_file(group_path):
    bld_file_path = os.path.join(group_path, 'bld.inf')
    bld_file = open(bld_file_path, 'r')

    build_platform = ''
    mmp_files = []
    should_read = True

    while True:
        if should_read:
            line = bld_file.readline()

        if not line:
            break

        line = line.rstrip().upper()

        if line == 'PRJ_PLATFORMS':
            should_read = True

            # Read only one platform
            build_platform = bld_file.readline()

            if not build_platform:
                raise Exception('Error in bld.inf: PRJ_PLATFORMS has no platform specified!')

            build_platform = build_platform.rstrip()
        elif line == 'PRJ_MMPFILES':
            while True:
                line = bld_file.readline()

                if not line or line.startswith('PRJ_'):
                    should_read = False
                    break

                mmp_files.append(line.rstrip())

    if not build_platform or build_platform.upper() == 'DEFAULT':
        # Use our default platform
        if get_sdk_is_eka1():
            build_platform = 'armi'
        else:
            build_platform = 'GCCE'

    return [build_platform, mmp_files]


def build(group_folder, configuration):
    # Run bldmake bldfiles to generate ABLD file
    print('Running bldmake to generate ABLD file')
    result = subprocess.call('bldmake bldfiles')

    if result != 0:
        print('bldmake used to generate ABLD file returned with error code ', result)
        return

    # Abusing perl with abld, try to build now!
    # Running export first
    invoke_abld_command(['export'])

    # Try generating ABLD makefiles. Do some check first
    (build_platform, mmp_files) = parse_bld_file(group_folder)

    # For each MMP, try to see if we want to generate them or regenerate them
    for mmp_file in mmp_files:
        mmp_file_full_path = os.path.join(group_folder, mmp_file)

        if not os.path.exists(mmp_file_full_path):
            raise Exception('MMP file {} declared in bld.inf does not exist!'.format(mmp_file))

        (mmp_filename, ext) = os.path.splitext(mmp_file)

        if should_generate_abld_makefile(mmp_file_full_path, build_platform):
            # Generate them
            print('Generate ABLD makefiles')
            invoke_abld_command(['makefile', build_platform.lower(), mmp_filename])

    build_platform = build_platform.lower()

    # Make library
    invoke_abld_command(['library', build_platform])

    # Make resource...
    invoke_abld_command(['resource', build_platform, configuration])

    # Make our main target
    invoke_abld_command(['target', build_platform, configuration])

    # Finalize
    invoke_abld_command(['final', build_platform, configuration])

    # Also build test
    invoke_abld_command(['test', 'export'])


# Copy build result of each MMP
def copy_build_result(group_folder, configuration, folder):
    (build_platform, mmp_files) = parse_bld_file(group_folder)

    for mmp_filename in mmp_files:
        (mmp_name, ext) = os.path.splitext(mmp_filename)
        (drive, group_folder_abs) = os.path.splitdrive(group_folder)
        binary_folder = os.path.join(drive, os.environ['EPOCROOT'], 'epoc32\\release\\')

        inside_folder_name = '{}\\{}\\'.format(build_platform.upper(), configuration)

        original_build_folder = os.path.join(binary_folder, inside_folder_name)
        target_build_folder = folder

        # Make new directories!!!
        try:
            os.makedirs(target_build_folder)
        except OSError:
            pass

        mmp_name = '{}.dll'.format(mmp_name)
        source_mmp_name = os.path.join(original_build_folder, mmp_name)
        target_mmp_name = os.path.join(target_build_folder, mmp_name)

        copyfile(source_mmp_name, target_mmp_name)


def build_entry(argvs):
    common_path = find_symbian_common_path()
    current_path = os.getcwd()

    (device, olddevice, group_folder, configuration, result_folder, should_continue) = parse_system_arguments(common_path, argvs)

    if not should_continue:
        return

    print('build.py - 2020 EKA2L1 project')
    print('Build Symbian project dependently from Carbide C++')
    print('=======================================================')

    print('Using the following device to build the project:')
    print(device)

    try:
        group_folder = os.path.abspath(group_folder)

        # Remove device letters because all the tools dont like it
        (drive, abs_path) = os.path.splitdrive(device[1])
        (group_drive, abs_path_drive) = os.path.splitdrive(group_folder)
        
        if drive.lower() != group_drive.lower():
            print('Drive of the SDK versus drive of the project to build are not the same')
            return 0

        # Yes it also want separator at the end
        if not (abs_path.endswith('\\') or abs_path.endswith('/')):
            abs_path += '\\'

        # Set the EPOCROOT environment variable
        os.environ['EPOCROOT'] = abs_path

        # Change current directory to group folder
        os.chdir(group_folder)

        # Ready to build!
        time_start_build = timeit.default_timer()
        build(group_folder, configuration)

        print('Build finished in {} seconds'.format(timeit.default_timer() - time_start_build))

        if result_folder:
            print('Copy build results to {}'.format(os.path.abspath(result_folder)))
            copy_build_result(group_folder, configuration, result_folder)
    except Exception as e:
        print(e)

    os.chdir(current_path)
    set_default_device(common_path, olddevice, device)

    return 0


# Main body
if __name__ == '__main__':
    build_entry(sys.argv)
    sys.exit(0)
