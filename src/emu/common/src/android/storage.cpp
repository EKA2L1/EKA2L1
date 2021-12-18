// Copyright (c) 2012- PPSSPP Project.
// Copyright (c) 2021- EKA2L1 Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include <common/android/storage.h>
#include <common/android/jniutils.h>
#include <common/log.h>

namespace eka2l1::common::android {
    static jmethodID open_content_uri_method_id;
    static jmethodID list_content_uri_dir_method_id;
    static jmethodID content_uri_create_file_method_id;
    static jmethodID content_uri_create_directory_method_id;
    static jmethodID content_uri_copy_file_method_id;
    static jmethodID content_uri_move_file_method_id;
    static jmethodID content_uri_remove_file_method_id;
    static jmethodID content_uri_rename_file_to_method_id;
    static jmethodID content_uri_get_file_info_method_id;
    static jmethodID content_uri_file_exists_method_id;
    static jmethodID is_external_storage_preserved_legacy_method_id;
    static jclass emulator_clazz;

    static jobject native_activity;

    std::string external_files_dir;
    std::string external_dir;
    
    void set_external_files_dir(const std::string &dir) {
        external_files_dir = dir;
    }
    
    void set_external_dir(const std::string &dir) {
        external_dir = dir;
    }

    std::string get_external_files_dir() {
        return external_files_dir;
    }
    
    std::string get_external_dir() {
        return external_dir;
    }

#define CHECK_METHOD_GET_OK(x) if (x == nullptr) return;

    void register_storage_callbacks(JNIEnv *env) {
        jclass temp_clazz = jni::find_class("com/github/eka2l1/emu/Emulator");
        emulator_clazz = (jclass) env->NewGlobalRef(temp_clazz);  
        env->DeleteLocalRef(temp_clazz);

        open_content_uri_method_id = env->GetStaticMethodID(emulator_clazz, "openContentUri",
                                          "(Ljava/lang/String;Ljava/lang/String;)I");
        CHECK_METHOD_GET_OK(open_content_uri_method_id);
        list_content_uri_dir_method_id = env->GetStaticMethodID(emulator_clazz, "listContentUriDir",
                                             "(Ljava/lang/String;)[Ljava/lang/String;");
        CHECK_METHOD_GET_OK(list_content_uri_dir_method_id);
        content_uri_create_directory_method_id = env->GetStaticMethodID(emulator_clazz,
                                                     "contentUriCreateDirectory",
                                                     "(Ljava/lang/String;Ljava/lang/String;)I");
        CHECK_METHOD_GET_OK(content_uri_create_directory_method_id);
        content_uri_create_file_method_id = env->GetStaticMethodID(emulator_clazz,
                                                "contentUriCreateFile",
                                                "(Ljava/lang/String;Ljava/lang/String;)I");
        CHECK_METHOD_GET_OK(content_uri_create_file_method_id);
        content_uri_copy_file_method_id = env->GetStaticMethodID(emulator_clazz, "contentUriCopyFile",
                                              "(Ljava/lang/String;Ljava/lang/String;)I");
        CHECK_METHOD_GET_OK(content_uri_copy_file_method_id);
        content_uri_remove_file_method_id = env->GetStaticMethodID(emulator_clazz, "contentUriRemoveFile",
                                                "(Ljava/lang/String;)I");
        CHECK_METHOD_GET_OK(content_uri_remove_file_method_id);
        content_uri_move_file_method_id = env->GetStaticMethodID(emulator_clazz, "contentUriMoveFile",
                                              "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I");
        CHECK_METHOD_GET_OK(content_uri_move_file_method_id);
        content_uri_rename_file_to_method_id = env->GetStaticMethodID(emulator_clazz,
                                                  "contentUriRenameFileTo",
                                                  "(Ljava/lang/String;Ljava/lang/String;)I");
        CHECK_METHOD_GET_OK(content_uri_rename_file_to_method_id);
        content_uri_get_file_info_method_id = env->GetStaticMethodID(emulator_clazz,
                                                 "contentUriGetFileInfo",
                                                 "(Ljava/lang/String;)Ljava/lang/String;");
        CHECK_METHOD_GET_OK(content_uri_get_file_info_method_id);
        content_uri_file_exists_method_id = env->GetStaticMethodID(emulator_clazz,
                                               "contentUriFileExists",
                                               "(Ljava/lang/String;)Z");
        CHECK_METHOD_GET_OK(content_uri_file_exists_method_id);
        is_external_storage_preserved_legacy_method_id = env->GetStaticMethodID(emulator_clazz,
                                                            "isExternalStoragePreservedLegacy",
                                                            "()Z");
        CHECK_METHOD_GET_OK(is_external_storage_preserved_legacy_method_id);
    }

    int open_content_uri_fd(const std::string &filename, open_content_uri_mode mode) {
        std::string fname = filename;

        // PPSSPP adds an ending slash to directories before looking them up.
        // TODO: Fix that in the caller (or don't call this for directories).
        if (fname.back() == '/')
            fname.pop_back();

        auto env = jni::environment();
        
        const char *modeStr = "";
        switch (mode) {
            case open_content_uri_mode::READ:
                modeStr = "r";
                break;
            case open_content_uri_mode::READ_WRITE:
                modeStr = "rw";
                break;
            case open_content_uri_mode::READ_WRITE_TRUNCATE:
                modeStr = "rwt";
                break;
        }
        jstring j_filename = env->NewStringUTF(fname.c_str());
        jstring j_mode = env->NewStringUTF(modeStr);
        int fd = env->CallStaticIntMethod(emulator_clazz, open_content_uri_method_id, j_filename, j_mode);
        return fd;
    }

    storage_error create_directory(const std::string &rootTreeUri, const std::string &dirName) {
        auto env = jni::environment();
        jstring paramRoot = env->NewStringUTF(rootTreeUri.c_str());
        jstring paramDirName = env->NewStringUTF(dirName.c_str());

        return storage_error_from_int(
                env->CallStaticIntMethod(emulator_clazz, content_uri_create_directory_method_id,
                                         paramRoot, paramDirName));
    }

    storage_error create_file(const std::string &parentTreeUri, const std::string &fileName) {
        auto env = jni::environment();
        jstring paramRoot = env->NewStringUTF(parentTreeUri.c_str());
        jstring paramFileName = env->NewStringUTF(fileName.c_str());
        return storage_error_from_int(
                env->CallStaticIntMethod(emulator_clazz, content_uri_create_file_method_id, paramRoot,
                                   paramFileName));
    }

    storage_error copy_file(const std::string &fileUri, const std::string &destParentUri) {
        auto env = jni::environment();
        jstring paramFileName = env->NewStringUTF(fileUri.c_str());
        jstring paramDestParentUri = env->NewStringUTF(destParentUri.c_str());
        return storage_error_from_int(
                env->CallStaticIntMethod(emulator_clazz, content_uri_copy_file_method_id,
                                         paramFileName, paramDestParentUri));
    }

    storage_error move_file(const std::string &fileUri, const std::string &srcParentUri,
                                  const std::string &destParentUri) {
        auto env = jni::environment();
        jstring paramFileName = env->NewStringUTF(fileUri.c_str());
        jstring paramSrcParentUri = env->NewStringUTF(srcParentUri.c_str());
        jstring paramDestParentUri = env->NewStringUTF(destParentUri.c_str());
        return storage_error_from_int(
                env->CallStaticIntMethod(emulator_clazz, content_uri_move_file_method_id,
                                         paramFileName, paramSrcParentUri, paramDestParentUri));
    }

    storage_error remove_file(const std::string &fileUri) {
        auto env = jni::environment();
        jstring paramFileName = env->NewStringUTF(fileUri.c_str());
        return storage_error_from_int(
                env->CallStaticIntMethod(emulator_clazz, content_uri_remove_file_method_id, paramFileName));
    }

    storage_error rename_file_to(const std::string &fileUri, const std::string &newName) {
        auto env = jni::environment();
        jstring paramFileUri = env->NewStringUTF(fileUri.c_str());
        jstring paramNewName = env->NewStringUTF(newName.c_str());
        return storage_error_from_int(
                env->CallStaticIntMethod(emulator_clazz, content_uri_rename_file_to_method_id,
                                         paramFileUri, paramNewName));
    }

    std::optional<std::string> get_file_info_as_string(const std::string &fileUri) {
        auto env = jni::environment();
        jstring paramFileUri = env->NewStringUTF(fileUri.c_str());

        jstring str = (jstring) env->CallStaticObjectMethod(emulator_clazz,
                                                      content_uri_get_file_info_method_id,
                                                      paramFileUri);
        if (!str) {
            return std::nullopt;
        }

        const char *charArray = env->GetStringUTFChars(str, 0);
        std::string result(charArray);
        env->DeleteLocalRef(str);

        return result;
    }

    bool file_exists(const std::string &fileUri) {
        auto env = jni::environment();
        jstring paramFileUri = env->NewStringUTF(fileUri.c_str());
        bool exists = env->CallStaticBooleanMethod(emulator_clazz, content_uri_file_exists_method_id,
                                                   paramFileUri);
        return exists;
    }

    std::vector<std::string> list_content_uri(const std::string &path) {
        auto env = jni::environment();

        jstring param = env->NewStringUTF(path.c_str());
        jobject retval = env->CallStaticObjectMethod(emulator_clazz, list_content_uri_dir_method_id,
                                                     param);

        jobjectArray fileList = (jobjectArray) retval;
        std::vector<std::string> items;

        int size = env->GetArrayLength(fileList);
        for (int i = 0; i < size; i++) {
            jstring str = (jstring) env->GetObjectArrayElement(fileList, i);
            const char *charArray = env->GetStringUTFChars(str, 0);
            if (charArray) {  // paranoia
                items.push_back(std::string(charArray));
            }
            env->ReleaseStringUTFChars(str, charArray);
            env->DeleteLocalRef(str);
        }
        env->DeleteLocalRef(fileList);

        return items;
    }

    bool is_external_storage_preserved_legacy() {
        auto env = jni::environment();
        return env->CallStaticBooleanMethod(emulator_clazz,
                                            is_external_storage_preserved_legacy_method_id);
    }

    const char *error_to_string(storage_error error) {
        switch (error) {
            case storage_error::SUCCESS:
                return "SUCCESS";
            case storage_error::UNKNOWN:
                return "UNKNOWN";
            case storage_error::NOT_FOUND:
                return "NOT_FOUND";
            case storage_error::DISK_FULL:
                return "DISK_FULL";
            case storage_error::ALREADY_EXISTS:
                return "ALREADY_EXISTS";
            default:
                return "(UNKNOWN)";
        }
    }
}