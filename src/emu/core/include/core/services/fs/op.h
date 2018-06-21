/*
 * Copyright (c) 2018 EKA2L1 Team / 2009 Nokia
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

enum TFsMessage {
    EFsAddFileSystem, ///< Adds a file system
    EFsRemoveFileSystem, ///< Removes a file system
    EFsMountFileSystem, ///< Mounts a file system
    EFsNotifyChange, ///< Notifies file and/or directory change
    EFsNotifyChangeCancel, ///< Cancels change notification
    EFsDriveList, ///< Gets a list of the available drive
    EFsDrive, ///< Gets information about a drive and the medium mounted on it
    EFsVolume, ///< Gets volume information for a formatted device
    EFsSetVolume, ///< Sets the label for a volume
    EFsSubst, ///< Gets the path assigned to a drive letter
    EFsSetSubst, ///< -- 10, Assigns a path to a drive letter
    EFsRealName, ///< Gets the real name of a file
    EFsDefaultPath, ///< Gets the system default path
    EFsSetDefaultPath, ///< Sets the system default path
    EFsSessionPath, ///< Gets the session path
    EFsSetSessionPath, ///< Sets the session path for the current file server client
    EFsMkDir, ///< Makes directory
    EFsRmDir, ///< Removes a directory
    EFsParse, ///< Parses a filename specification
    EFsDelete, ///< Deletes file
    EFsRename, ///< -- 20 Renames a single file or directory
    EFsReplace, ///< Replaces a single file with another
    EFsEntry, ///< Gets a file's attributes
    EFsSetEntry, ///< Sets both the attributes and the last modified date and time for a file or directory
    EFsGetDriveName, ///<  Gets the name of a drive
    EFsSetDriveName, ///< Sets the name of a drive
    EFsFormatSubClose, ///< Closes the Format subsession
    EFsDirSubClose, ///< Closes the directory.
    EFsFileSubClose, ///< Closes the file
    EFsRawSubClose, ///< Closes the direct access channel to the disk
    EFsFileOpen, ///< -- 30 Opens file
    EFsFileCreate, ///< Creates and opens a new file
    EFsFileReplace, ///< Replaces a file of the same name or creates a new file
    EFsFileTemp, ///< Creates and opens a temporary file
    EFsFileRead, ///< Reads from the file
    EFsFileWrite, ///< Writes to the file
    EFsFileLock, ///< Locks a region within the file
    EFsFileUnLock, ///< Unlocks a region within the file
    EFsFileSeek, ///< Sets the the current file position
    EFsFileFlush, ///< Commits data to the storage device
    EFsFileSize, ///< -- 40 Gets the current file size
    EFsFileSetSize, ///< Sets the file size
    EFsFileAtt, ///< Gets the file's attributes
    EFsFileSetAtt, ///< Sets or clears file attributes
    EFsFileModified, ///< Gets local date and time the file was last modified
    EFsFileSetModified, ///< Sets the date and time the file was last modified
    EFsFileSet, ///< Sets the file’s attributes, last modification date/time
    EFsFileChangeMode, ///< Switches an open file's access mode
    EFsFileRename, ///< Renames a file
    EFsDirOpen, ///< Opens a directory
    EFsDirReadOne, ///< -- 50 Reads a single directory entry
    EFsDirReadPacked, ///< Reads all filtered directory entries
    EFsFormatOpen, ///< Opens a device for formatting
    EFsFormatNext, ///< Executes the next format step
    EFsRawDiskOpen, ///< Opens a direct access channel to the disk */
    EFsRawDiskRead, ///< Reads directly from the disk
    EFsRawDiskWrite, ///< Writes directly to the disk
    EFsResourceCountMarkStart, ///< Marks the start of resource count checking
    EFsResourceCountMarkEnd, ///< Ends resource count checking
    EFsResourceCount, ///< Gets the number of currently open resources
    EFsCheckDisk, ///< -- 60 Checks the integrity of the disk on the specified drive
    EFsGetShortName, ///< Gets the short filename
    EFsGetLongName, ///< Gets the long filename
    EFsIsFileOpen, ///< Tests whether a file is open
    EFsListOpenFiles, ///< get a list of open files */
    EFsGetNotifyUser, ///< Tests user notification of file access failure is in effect
    EFsSetNotifyUser, ///< Sets if the user should be notified of file access failure
    EFsIsFileInRom, ///< Gets a pointer to the specified file, if it is in ROM
    EFsIsValidName, ///< Tests whether a filename and path are syntactically correct
    EFsDebugFunction, ///< Different debugging info
    EFsReadFileSection, ///< -- 70 Reads data from a file without opening it
    EFsNotifyChangeEx, ///< Requests a notification of change to files or directories
    EFsNotifyChangeCancelEx, ///< Cancels all outstanding requests for notification of change
    EFsDismountFileSystem, ///< Dismounts the file system from the specified drive
    EFsFileSystemName, ///< Gets the name of the file system mounted on the specified drive
    EFsScanDrive, ///< Checks the specified drive for specific errors and corrects them
    EFsControlIo, ///< General purpose test interface
    EFsLockDrive, ///< Locks a MultiMedia card in the specified drive
    EFsUnlockDrive, ///< Unlocks the MultiMedia card in the specified drive
    EFsClearPassword, ///< Clears the password from the locked MultiMedia card
    EFsNotifyDiskSpace, ///< -- 80 Disk space change notification
    EFsNotifyDiskSpaceCancel, ///< Cancels a specific outstanding notification
    EFsFileDrive, ///< Gets drive information on which this file resides
    EFsRemountDrive, ///< Forces a remount of the specified drive
    EFsMountFileSystemScan, ///< Mounts a file system and performs a scan on a drive
    EFsSessionToPrivate, ///< Sets the session path to point to the private path
    EFsPrivatePath, ///< Creates the text defining the private path
    EFsCreatePrivatePath, ///< Creates the private path for a process
    EFsAddExtension, ///< Adds the specified extension
    EFsMountExtension, ///< Mounts the the specified extension
    EFsDismountExtension, ///< -- 90 Dismounts the specified extension
    EFsRemoveExtension, ///< Removes the specified extension
    EFsExtensionName, ///< Gets the name of the extension on the specified drive
    EFsStartupInitComplete, ///< Noifies the file server of startup initialisation completion
    EFsSetLocalDriveMapping, ///< Set the local drive mapping
    EFsFinaliseDrive, ///< Finalise a specific drive
    EFsFileDuplicate, ///< Makes a duplicate of this file handle
    EFsFileAdopt, ///< Adopts an already open file
    EFsSwapFileSystem, ///< Swaps file systems
    EFsErasePassword, ///< Erase the password from the locked MultiMedia card
    EFsReserveDriveSpace, ///< -- 100 Reserves an area of a drive
    EFsGetReserveAccess, ///< Get exclusive access to reserved area
    EFsReleaseReserveAccess, ///< Release exclusive access to reserved area
    EFsFileName, ///< Gets the final part of a filename
    EFsGetMediaSerialNumber, ///<  Gets the serial number of media
    EFsFileFullName, ///< Gets the full filename
    EFsAddPlugin, ///< Adds the specified plugin
    EFsRemovePlugin, ///< Removes the specified plugin
    EFsMountPlugin, ///< Mounts the specified plugin
    EFsDismountPlugin, ///< Dismounts the specified plugin
    EFsPluginName, ///<-- 110 Gets a plugin's name in specific position and drive
    EFsPluginOpen, ///< Opens the plugin
    EFsPluginSubClose, ///< Closes the plugin
    EFsPluginDoRequest, ///< Issues an asynchronous plugin request
    EFsPluginDoControl, ///< Issues a synchronous plugin request
    EFsPluginDoCancel, ///< Cancels an synchronous plugin request
    EFsNotifyDismount, ///< Issues a request to asynchronously dismount the file system
    EFsNotifyDismountCancel, ///< Cancels a request to asynchronously dismount the file system
    EFsAllowDismount, ///< Notifies that it is safe to dismount the file system
    EFsSetStartupConfiguration, ///< Configures file server at startup
    EFsFileReadCancel, ///< -- 120 Cancels an outstanding asynchronous read request
    EFsAddCompositeMount, ///< Add a mount to the composite file system
    EFsSetSessionFlags, ///< Set/Clear session specific flags
    EFsSetSystemDrive, ///< Set SystemDrive
    EFsBlockMap, ///< Fetches the BlockMap of a file
    EFsUnclamp, ///< Re-enable modification of a specified file in storage media
    EFsFileClamp, ///< Disable modification of a specified file in storage media
    EFsQueryVolumeInfoExt, ///< Query interface to retrieve extended volume information
    EFsInitialisePropertiesFile, ///< Read in the F32 properties file provided by ESTART
    EFsFileWriteDirty, ///< Writes dirty data to disk. Used when write caching enabled
    EFsSynchroniseDriveThread, ///< -- 130 Synchronises the asynchronous operation which executes in driver thread
    EFsAddProxyDrive, ///< Loads  a proxy drive
    EFsRemoveProxyDrive, ///< Unloads a proxy drive
    EFsMountProxyDrive, ///< Mounts a proxy drive
    EFsDismountProxyDrive, ///< Dismounts a proxy drive
    EFsNotificationOpen, ///< Opens the notification
    EFsNotificationBuffer, ///< Communicates buffer to file server
    EFsNotificationRequest, ///< Sends the notification request
    EFsNotificationCancel, ///< Cancels the notification request
    EFsNotificationSubClose, ///< Closes the notification
    EFsNotificationAdd, ///< -- 140 Adds filter to the server, comprising a path and notification type
    EFsNotificationRemove, ///< Removes filters from Server-Side
    EFsLoadCodePage, ///< Loads a code page library
    EMaxClientOperations ///< This must always be the last operation insert above
};